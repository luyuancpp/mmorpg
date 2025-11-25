package _go

import (
	"encoding/json"
	"google.golang.org/protobuf/reflect/protodesc"
	"google.golang.org/protobuf/reflect/protoregistry"
	"log"
	"os"
	"path/filepath"
	"pbgen/config"
	utils2 "pbgen/internal/utils"
	"strings"

	pbmysql "github.com/luyuancpp/proto2mysql"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
	"google.golang.org/protobuf/types/dynamicpb"

	"pbgen/internal"
)

// MessageListConfig 定义消息名列表结构
type MessageListConfig struct {
	Messages []string `json:"messages"`
}

// extractMessageNamesFromProto 提取消息全限定名（适配 v1.36.6）
func extractMessageNamesFromProto(protoFile string) ([]string, error) {
	var messageNames []string

	for _, fileDesc := range internal.FdSet.GetFile() {
		// 修复：用 strings.HasSuffix 可能匹配到同名文件，建议用绝对路径或精确匹配
		// 例如：若 protoFile 是 "mysql_database_table.proto"，精确匹配文件名
		fileName := filepath.Base(fileDesc.GetName())
		if fileName != protoFile {
			continue
		}

		pkgName := fileDesc.GetPackage()
		for _, msgDesc := range fileDesc.GetMessageType() {
			var fullName string
			if pkgName == "" {
				fullName = msgDesc.GetName() // 无包名，直接用消息名
			} else {
				fullName = pkgName + "." + msgDesc.GetName()
			}
			messageNames = append(messageNames, fullName)
			log.Printf("从 %s 提取消息全限定名: %s", fileDesc.GetName(), fullName)
		}
	}

	if len(messageNames) == 0 {
		log.Printf("警告：未从文件 %s 中提取到任何消息（检查文件名是否匹配）", protoFile)
	}
	return messageNames, nil
}

// LoadAllDescriptors 激活描述符（v1.36.6 专用，无需 protoregistry.NewFiles()）

// LoadAllDescriptors 适配你的 protoregistry 版本：用 Files 管理 + protodesc 激活
func LoadAllDescriptors() error {
	internal.LoadMutex.Lock()
	defer internal.LoadMutex.Unlock()

	if internal.DescriptorsLoaded {
		return nil
	}

	log.Printf("=== 开始激活描述符（共 %d 个文件）===", len(internal.FdSet.GetFile()))

	// 1. 初始化 protoregistry.Files（作为 FileResolver，用于解析跨文件依赖）
	fileReg := &protoregistry.Files{}

	// 2. 关键修复：传入 fileReg 作为 FileResolver，支持依赖解析
	for _, rawFile := range internal.FdSet.GetFile() {
		// 第二个参数传入 fileReg，而非 nil！让 protodesc 能从已注册的文件中找依赖
		activeFileDesc, err := protodesc.NewFile(rawFile, fileReg)
		if err != nil {
			// 打印详细依赖错误，便于定位缺失的依赖
			log.Printf("激活文件 %s 失败: 依赖解析错误=%v，跳过", rawFile.GetName(), err)
			continue
		}

		// 3. 注册到 fileReg（此时依赖已激活，注册会成功）
		if err := fileReg.RegisterFile(activeFileDesc); err != nil {
			log.Printf("注册文件 %s 到 registry 失败: %v，跳过", activeFileDesc.Path(), err)
			continue
		}

		// 4. 缓存文件描述符
		internal.FileDescCache[rawFile.GetName()] = activeFileDesc
		log.Printf("已激活并注册文件: %s（包名: %s，消息数: %d）",
			activeFileDesc.Path(), activeFileDesc.Package(), activeFileDesc.Messages().Len())
	}

	// 5. 重新缓存消息描述符（此时依赖已解决，消息能正常提取）
	internal.ActiveMsgDescCache = make(map[protoreflect.FullName]protoreflect.MessageDescriptor) // 清空旧缓存
	fileReg.RangeFiles(func(activeFileDesc protoreflect.FileDescriptor) bool {
		messages := activeFileDesc.Messages()
		for i := 0; i < messages.Len(); i++ {
			activeMsgDesc := messages.Get(i)
			// 生成全限定名（无包名则直接用消息名）
			var fullNameStr string
			if pkg := string(activeFileDesc.Package()); pkg != "" {
				fullNameStr = pkg + "." + string(activeMsgDesc.Name())
			} else {
				fullNameStr = string(activeMsgDesc.Name())
			}
			fullName := protoreflect.FullName(fullNameStr)

			internal.ActiveMsgDescCache[fullName] = activeMsgDesc
			log.Printf("  缓存消息: %s（字段数: %d）", fullNameStr, activeMsgDesc.Fields().Len())
		}
		return true
	})

	internal.DescriptorsLoaded = true
	log.Printf("=== 描述符激活完成（共缓存 %d 个消息）===", len(internal.ActiveMsgDescCache))
	return nil
}

// 以下函数保持不变（GenerateMergedTableSQL、verifyMessageValidity 等）
func GenerateMergedTableSQL(messageNames []string) error {
	if err := LoadAllDescriptors(); err != nil {
		return err
	}

	sqlGenerator := pbmysql.NewPbMysqlDB()
	var mergedSQL strings.Builder

	for _, msgFullNameStr := range messageNames {
		msgFullName := protoreflect.FullName(msgFullNameStr)

		// 从缓存获取激活后的消息描述符
		activeMsgDesc, exists := internal.ActiveMsgDescCache[msgFullName]
		if !exists {
			log.Printf("警告: 未找到激活的消息描述符 %s，跳过", msgFullNameStr)
			continue
		}

		// 创建包含完整字段的消息实例
		msgInstance := dynamicpb.NewMessage(activeMsgDesc)
		if msgInstance == nil {
			log.Printf("警告: 无法创建消息 %s 的实例，跳过", msgFullNameStr)
			continue
		}
		log.Printf("已创建消息实例: %s", msgFullNameStr)

		// 验证实例字段
		verifyMessageValidity(msgFullNameStr, msgInstance)

		// 生成 SQL
		sqlGenerator.RegisterTable(msgInstance)
		tableSQL := sqlGenerator.GetCreateTableSQL(msgInstance)
		mergedSQL.WriteString(tableSQL)
		mergedSQL.WriteString("\n\n")
	}

	// 写入 SQL 文件（逻辑不变）
	if mergedSQL.Len() == 0 {
		log.Println("提示: 未生成任何SQL（无有效消息实例）")
		return nil
	}

	for _, protoDir := range config.ProtoDirs {
		if !utils2.HasGrpcService(protoDir) {
			continue
		}

		sqlDir := utils2.BuildModelPath(protoDir)
		if err := os.MkdirAll(sqlDir, 0755); err != nil {
			return err
		}

		sqlFileName := config.ModelSqlExtension

		sqlPath := filepath.Join(sqlDir, sqlFileName)

		if err := os.WriteFile(sqlPath, []byte(mergedSQL.String()), 0644); err != nil {
			return err
		}
		log.Printf("SQL文件生成成功: %s", sqlPath)
	}

	return nil
}

func verifyMessageValidity(msgName string, msg proto.Message) {
	msgReflect := msg.ProtoReflect()
	fieldCount := 0

	fields := msgReflect.Descriptor().Fields()
	for i := 0; i < fields.Len(); i++ {
		fd := fields.Get(i)
		fieldCount++
		log.Printf("消息 %s 实例字段: %s (类型: %s, 编号: %d)",
			msgName, fd.Name(), fd.Kind(), fd.Number())
	}

	if fieldCount == 0 {
		log.Printf("警告: 消息 %s 无任何字段（激活失败）", msgName)
	} else {
		log.Printf("消息 %s 验证通过：共包含 %d 个字段", msgName, fieldCount)
	}
}

// 其他辅助函数（writeMessageNamesToJSON、GenerateDBResource 等）保持不变
func writeMessageNamesToJSON(messages []string) error {
	data, err := json.MarshalIndent(&MessageListConfig{Messages: messages}, "", "  ")
	if err != nil {
		return err
	}

	outputPath := config.TableGeneratorPath + config.DBTableMessageListJson
	if err := os.WriteFile(outputPath, data, 0644); err != nil {
		return err
	}

	log.Printf("配置文件生成成功: %s", outputPath)
	return nil
}

func getFullMessageName(pkgName, msgName string) string {
	if pkgName == "" {
		return msgName
	}
	return pkgName + "." + msgName
}

func GenerateDBResource() {
	utils2.Wg.Add(1)
	go func() {
		defer utils2.Wg.Done()

		protoFile := config.DbTableName
		log.Printf("开始处理目标文件: %s", protoFile)

		messageNames, err := extractMessageNamesFromProto(protoFile)
		if err != nil {
			log.Fatalf("提取消息名失败: %v", err)
		}
		if len(messageNames) == 0 {
			log.Printf("未从 %s 中提取到任何消息", protoFile)
		}

		// 并发生成 JSON 配置
		utils2.Wg.Add(1)
		go func() {
			defer utils2.Wg.Done()
			if err := writeMessageNamesToJSON(messageNames); err != nil {
				log.Fatalf("生成JSON配置失败: %v", err)
			}
		}()

		// 并发生成 SQL
		utils2.Wg.Add(1)
		go func() {
			defer utils2.Wg.Done()
			if err := GenerateMergedTableSQL(messageNames); err != nil {
				log.Fatalf("生成SQL失败: %v", err)
			}
		}()
	}()
}
