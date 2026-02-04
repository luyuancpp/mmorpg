package _go

import (
	"encoding/json"
	"os"
	"path/filepath"
	"strings"
	"sync"

	"github.com/luyuancpp/proto2mysql"
	"go.uber.org/zap" // 引入zap用于结构化日志字段
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protodesc"
	"google.golang.org/protobuf/reflect/protoreflect"
	"google.golang.org/protobuf/reflect/protoregistry"
	"google.golang.org/protobuf/types/dynamicpb"

	"pbgen/global_value"
	"pbgen/internal"
	_config "pbgen/internal/config"
	utils2 "pbgen/internal/utils"
	"pbgen/logger" // 引入全局logger包
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
			logger.Global.Info("提取消息全限定名",
				zap.String("proto_file", fileDesc.GetName()),
				zap.String("message_full_name", fullName),
				zap.String("target_proto_file", protoFile),
			)
		}
	}

	if len(messageNames) == 0 {
		logger.Global.Warn("未提取到任何消息",
			zap.String("target_proto_file", protoFile),
			zap.Int("total_files_scanned", len(internal.FdSet.GetFile())),
		)
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

	logger.Global.Info("开始激活描述符",
		zap.Int("total_files", len(internal.FdSet.GetFile())),
	)

	// 1. 初始化 protoregistry.Files（作为 FileResolver，用于解析跨文件依赖）
	fileReg := &protoregistry.Files{}

	// 2. 关键修复：传入 fileReg 作为 FileResolver，支持依赖解析
	for _, rawFile := range internal.FdSet.GetFile() {
		// 第二个参数传入 fileReg，而非 nil！让 protodesc 能从已注册的文件中找依赖
		activeFileDesc, err := protodesc.NewFile(rawFile, fileReg)
		if err != nil {
			// 打印详细依赖错误，便于定位缺失的依赖
			logger.Global.Warn("激活文件失败，跳过",
				zap.String("file_name", rawFile.GetName()),
				zap.Error(err),
			)
			continue
		}

		// 3. 注册到 fileReg（此时依赖已激活，注册会成功）
		if err := fileReg.RegisterFile(activeFileDesc); err != nil {
			logger.Global.Warn("注册文件到registry失败，跳过",
				zap.String("file_path", activeFileDesc.Path()),
				zap.Error(err),
			)
			continue
		}

		// 4. 缓存文件描述符
		internal.FileDescCache[rawFile.GetName()] = activeFileDesc
		logger.Global.Debug("已激活并注册文件",
			zap.String("file_path", activeFileDesc.Path()),
			zap.String("package", string(activeFileDesc.Package())),
			zap.Int("message_count", activeFileDesc.Messages().Len()),
		)
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
			logger.Global.Debug("缓存消息描述符",
				zap.String("message_full_name", fullNameStr),
				zap.Int("field_count", activeMsgDesc.Fields().Len()),
				zap.String("file_path", string(activeFileDesc.Path())),
			)
		}
		return true
	})

	internal.DescriptorsLoaded = true
	logger.Global.Info("描述符激活完成",
		zap.Int("cached_message_count", len(internal.ActiveMsgDescCache)),
		zap.Int("processed_file_count", len(internal.FileDescCache)),
	)
	return nil
}

// 以下函数保持不变（GenerateMergedTableSQL、verifyMessageValidity 等）
func GenerateMergedTableSQL(messageNames []string) error {
	if err := LoadAllDescriptors(); err != nil {
		return err
	}

	sqlGenerator := proto2mysql.NewPbMysqlDB()
	var mergedSQL strings.Builder

	for _, msgFullNameStr := range messageNames {
		msgFullName := protoreflect.FullName(msgFullNameStr)

		// 从缓存获取激活后的消息描述符
		activeMsgDesc, exists := internal.ActiveMsgDescCache[msgFullName]
		if !exists {
			logger.Global.Warn("未找到激活的消息描述符，跳过",
				zap.String("message_full_name", msgFullNameStr),
			)
			continue
		}

		// 创建包含完整字段的消息实例
		msgInstance := dynamicpb.NewMessage(activeMsgDesc)
		if msgInstance == nil {
			logger.Global.Warn("无法创建消息实例，跳过",
				zap.String("message_full_name", msgFullNameStr),
			)
			continue
		}
		logger.Global.Debug("创建消息实例成功",
			zap.String("message_full_name", msgFullNameStr),
		)

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
		logger.Global.Info("未生成任何SQL",
			zap.String("reason", "无有效消息实例"),
			zap.Int("input_message_count", len(messageNames)),
		)
		return nil
	}

	for _, protoDir := range global_value.ProtoDirs {
		if !utils2.HasGrpcService(protoDir) {
			continue
		}

		sqlDir := utils2.BuildModelGoPath(protoDir)
		if err := os.MkdirAll(sqlDir, 0755); err != nil {
			logger.Global.Error("创建SQL目录失败",
				zap.String("sql_dir", sqlDir),
				zap.String("proto_dir", protoDir),
				zap.Error(err),
			)
			return err
		}

		sqlFileName := _config.Global.FileExtensions.ModelSqlExtension
		sqlPath := filepath.Join(sqlDir, sqlFileName)

		if err := os.WriteFile(sqlPath, []byte(mergedSQL.String()), 0644); err != nil {
			logger.Global.Error("写入SQL文件失败",
				zap.String("sql_path", sqlPath),
				zap.Error(err),
			)
			return err
		}
		logger.Global.Info("SQL文件生成成功",
			zap.String("sql_path", sqlPath),
			zap.String("proto_dir", protoDir),
			zap.Int("sql_length", mergedSQL.Len()),
		)
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
		logger.Global.Debug("消息实例字段信息",
			zap.String("message_name", msgName),
			zap.String("field_name", string(fd.Name())),
			zap.String("field_kind", string(fd.Kind())),
			zap.Int32("field_number", int32(fd.Number())),
		)
	}

	if fieldCount == 0 {
		logger.Global.Warn("消息无任何字段",
			zap.String("message_name", msgName),
			zap.String("reason", "激活失败"),
		)
	} else {
		logger.Global.Info("消息验证通过",
			zap.String("message_name", msgName),
			zap.Int("field_count", fieldCount),
		)
	}
}

// 其他辅助函数（writeMessageNamesToJSON、GenerateDBResource 等）保持不变
func writeMessageNamesToJSON(messages []string) error {
	data, err := json.MarshalIndent(&MessageListConfig{Messages: messages}, "", "  ")
	if err != nil {
		logger.Global.Error("JSON序列化失败",
			zap.Error(err),
			zap.Int("message_count", len(messages)),
		)
		return err
	}

	outputPath := _config.Global.Paths.TableGeneratorDir + _config.Global.Naming.DbTableListJson
	if err := os.WriteFile(outputPath, data, 0644); err != nil {
		logger.Global.Error("写入JSON配置文件失败",
			zap.String("output_path", outputPath),
			zap.Error(err),
		)
		return err
	}

	logger.Global.Info("配置文件生成成功",
		zap.String("output_path", outputPath),
		zap.Int("message_count", len(messages)),
	)
	return nil
}

func getFullMessageName(pkgName, msgName string) string {
	if pkgName == "" {
		return msgName
	}
	return pkgName + "." + msgName
}

func GenerateDBResource(wg *sync.WaitGroup) {
	wg.Add(1)
	go func() {
		defer wg.Done()

		protoFile := _config.Global.Naming.DbTableFile
		logger.Global.Info("开始处理目标Proto文件",
			zap.String("proto_file", protoFile),
		)

		messageNames, err := extractMessageNamesFromProto(protoFile)
		if err != nil {
			logger.Global.Fatal("提取消息名失败",
				zap.String("proto_file", protoFile),
				zap.Error(err),
			)
		}
		if len(messageNames) == 0 {
			logger.Global.Info("未提取到任何消息",
				zap.String("proto_file", protoFile),
			)
		}

		// 并发生成 JSON 配置
		wg.Add(1)
		go func() {
			defer wg.Done()
			if err := writeMessageNamesToJSON(messageNames); err != nil {
				logger.Global.Fatal("生成JSON配置失败",
					zap.Error(err),
					zap.Int("message_count", len(messageNames)),
				)
			}
		}()

		// 并发生成 SQL
		wg.Add(1)
		go func() {
			defer wg.Done()
			if err := GenerateMergedTableSQL(messageNames); err != nil {
				logger.Global.Fatal("生成SQL失败",
					zap.Error(err),
					zap.Int("message_count", len(messageNames)),
				)
			}
		}()
	}()
}
