package database

import (
	"encoding/json"
	"google.golang.org/protobuf/reflect/protodesc"
	"google.golang.org/protobuf/reflect/protoregistry"
	"log"
	"os"
	"path/filepath"
	"strings"
	"sync"

	pbmysql "github.com/luyuancpp/pbmysql-go"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
	"google.golang.org/protobuf/types/dynamicpb"

	"pbgen/internal"
	"pbgen/internal/config"
	"pbgen/util"
)

// MessageListConfig 定义消息名列表结构
type MessageListConfig struct {
	Messages []string `json:"messages"`
}

var (
	activeMsgDescCache map[protoreflect.FullName]protoreflect.MessageDescriptor
	fileDescCache      map[string]protoreflect.FileDescriptor

	descriptorsLoaded bool
	loadMutex         sync.Mutex
)

// 初始化缓存
func init() {
	activeMsgDescCache = make(map[protoreflect.FullName]protoreflect.MessageDescriptor)
	fileDescCache = make(map[string]protoreflect.FileDescriptor)
}

// extractMessageNamesFromProto 提取消息全限定名（适配 v1.36.6）
func extractMessageNamesFromProto(protoFile string) ([]string, error) {
	var messageNames []string

	for _, fileDesc := range internal.FdSet.GetFile() {
		if !strings.HasSuffix(fileDesc.GetName(), protoFile) {
			continue
		}
		pkgName := fileDesc.GetPackage()
		for _, msgDesc := range fileDesc.GetMessageType() {
			// 拼接全限定名（包名.消息名，避免类型转换错误）
			var fullName string
			if pkgName == "" {
				fullName = msgDesc.GetName()
			} else {
				fullName = pkgName + "." + msgDesc.GetName()
			}
			messageNames = append(messageNames, fullName)
			log.Printf("已提取消息全限定名: %s", fullName)
		}
	}

	return messageNames, nil
}

// loadAllDescriptors 激活描述符（v1.36.6 专用，无需 protoregistry.NewFiles()）

func init() {
	activeMsgDescCache = make(map[protoreflect.FullName]protoreflect.MessageDescriptor)
	fileDescCache = make(map[string]protoreflect.FileDescriptor)
}

// loadAllDescriptors 适配你的 protoregistry 版本：用 Files 管理 + protodesc 激活
func loadAllDescriptors() error {
	loadMutex.Lock()
	defer loadMutex.Unlock()

	if descriptorsLoaded {
		return nil
	}

	log.Printf("=== 开始激活描述符（共 %d 个文件）===", len(internal.FdSet.GetFile()))

	// 1. 初始化 protoregistry.Files（你的版本有这个结构体，无 NewFiles() 则手动初始化）
	fileReg := &protoregistry.Files{} // 直接实例化，替代 NewFiles()

	// 2. 第一步：用 protodesc 激活所有原始文件描述符，并注册到 fileReg
	for _, rawFile := range internal.FdSet.GetFile() {
		// 2.1 激活单个原始文件描述符（你的版本有 protodesc.NewFile）
		// 注意：若有跨文件依赖，需先激活依赖文件（这里简化为按顺序激活，实际可按依赖排序）
		activeFileDesc, err := protodesc.NewFile(rawFile, nil)
		if err != nil {
			log.Printf("激活文件 %s 失败: %v，跳过", rawFile.GetName(), err)
			continue
		}

		// 2.2 将激活后的文件描述符注册到 protoregistry.Files 中
		if err := fileReg.RegisterFile(activeFileDesc); err != nil {
			log.Printf("注册文件 %s 到 registry 失败: %v，跳过", activeFileDesc.Path(), err)
			continue
		}

		// 2.3 缓存激活后的文件描述符
		fileDescCache[rawFile.GetName()] = activeFileDesc
		log.Printf("已激活并注册文件: %s（包名: %s）", activeFileDesc.Path(), activeFileDesc.Package())
	}

	// 3. 第二步：从 fileReg 中提取所有激活的消息描述符，缓存到 activeMsgDescCache
	// 遍历已注册的所有文件
	fileReg.RangeFiles(func(activeFileDesc protoreflect.FileDescriptor) bool {
		// 遍历文件中的所有顶层消息（你的版本用 Len() + Get() 遍历）
		messages := activeFileDesc.Messages()
		for i := 0; i < messages.Len(); i++ {
			activeMsgDesc := messages.Get(i)
			// 拼接消息全限定名（包名.消息名）
			fullNameStr := string(activeFileDesc.Package()) + "." + string(activeMsgDesc.Name())
			fullName := protoreflect.FullName(fullNameStr)

			// 缓存消息描述符
			activeMsgDescCache[fullName] = activeMsgDesc

			// 验证：打印消息字段（确认激活成功）
			fieldCount := 0
			fields := activeMsgDesc.Fields()
			for j := 0; j < fields.Len(); j++ {
				field := fields.Get(j)
				fieldCount++
				log.Printf("  消息 %s → 字段: %s（类型: %s, 编号: %d）",
					fullNameStr, field.Name(), field.Kind(), field.Number())
			}
			log.Printf("  已缓存消息: %s（字段数: %d）", fullNameStr, fieldCount)
		}
		return true // 继续遍历下一个文件
	})

	descriptorsLoaded = true
	log.Printf("=== 描述符激活完成（共缓存 %d 个消息）===", len(activeMsgDescCache))
	return nil
}

// 以下函数保持不变（GenerateMergedTableSQL、verifyMessageValidity 等）
func GenerateMergedTableSQL(messageNames []string) error {
	if err := loadAllDescriptors(); err != nil {
		return err
	}

	sqlGenerator := pbmysql.NewPbMysqlDB()
	var mergedSQL strings.Builder

	for _, msgFullNameStr := range messageNames {
		msgFullName := protoreflect.FullName(msgFullNameStr)

		// 从缓存获取激活后的消息描述符
		activeMsgDesc, exists := activeMsgDescCache[msgFullName]
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
		tableSQL := sqlGenerator.GetCreateTableSql(msgInstance)
		mergedSQL.WriteString(tableSQL)
		mergedSQL.WriteString("\n\n")
	}

	// 写入 SQL 文件（逻辑不变）
	if mergedSQL.Len() == 0 {
		log.Println("提示: 未生成任何SQL（无有效消息实例）")
		return nil
	}

	for _, protoDir := range config.ProtoDirs {
		if !util.HasGrpcService(protoDir) {
			continue
		}

		outputDir := util.BuildModelPath(protoDir)
		sqlDir := filepath.Join(outputDir, config.ModelPath)
		if err := os.MkdirAll(sqlDir, 0755); err != nil {
			return err
		}

		sqlFileName := config.SqlExtension
		if sqlFileName == "" {
			sqlFileName = "merged_table.sql"
		}
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
	util.Wg.Add(1)
	go func() {
		defer util.Wg.Done()

		protoFile := config.DbTableName
		log.Printf("开始处理目标文件: %s", protoFile)

		messageNames, err := extractMessageNamesFromProto(protoFile)
		if err != nil {
			log.Fatalf("提取消息名失败: %v", err)
		}
		if len(messageNames) == 0 {
			log.Fatalf("未从 %s 中提取到任何消息", protoFile)
		}

		// 并发生成 JSON 配置
		util.Wg.Add(1)
		go func() {
			defer util.Wg.Done()
			if err := writeMessageNamesToJSON(messageNames); err != nil {
				log.Fatalf("生成JSON配置失败: %v", err)
			}
		}()

		// 并发生成 SQL
		util.Wg.Add(1)
		go func() {
			defer util.Wg.Done()
			if err := GenerateMergedTableSQL(messageNames); err != nil {
				log.Fatalf("生成SQL失败: %v", err)
			}
		}()
	}()
}
