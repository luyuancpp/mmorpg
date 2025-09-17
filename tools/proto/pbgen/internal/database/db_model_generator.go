package database

import (
	"encoding/json"
	"log"
	"os"
	"strings"
	"sync"

	pbmysql "github.com/luyuancpp/pbmysql-go"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
	"google.golang.org/protobuf/types/descriptorpb"
	"google.golang.org/protobuf/types/dynamicpb" // 核心：动态创建消息实例的标准包

	"pbgen/internal"
	"pbgen/internal/config"
	"pbgen/util"
)

var (
	// 仅缓存消息描述符（最稳定的元数据载体）
	messageDescCache = make(map[string]*descriptorpb.DescriptorProto)
	fileDescCache    = make(map[string]*descriptorpb.FileDescriptorProto)

	descriptorsLoaded bool
	loadMutex         sync.Mutex
)

// MessageListConfig 定义消息名列表结构
type MessageListConfig struct {
	Messages []string `json:"messages"`
}

// extractMessageNamesFromProto 提取消息全限定名（包名.消息名）
func extractMessageNamesFromProto(protoFile string) ([]string, error) {
	var messageNames []string

	for _, fileDesc := range internal.FdSet.GetFile() {
		// 精确匹配目标proto文件（避免包含路径导致的误匹配）
		if !strings.HasSuffix(fileDesc.GetName(), protoFile) {
			continue
		}
		pkgName := fileDesc.GetPackage()
		for _, msgDesc := range fileDesc.GetMessageType() {
			fullName := getFullMessageName(pkgName, msgDesc.GetName())
			messageNames = append(messageNames, fullName)
			log.Printf("已提取消息: %s", fullName)
		}
	}

	return messageNames, nil
}

// writeMessageNamesToJSON 写入消息列表配置
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

// getFullMessageName 生成消息全限定名
func getFullMessageName(pkgName, msgName string) string {
	if pkgName == "" {
		return msgName
	}
	return pkgName + "." + msgName
}

// loadAllDescriptors 加载描述符并缓存（无外部依赖，最稳定）
func loadAllDescriptors() error {
	loadMutex.Lock()
	defer loadMutex.Unlock()

	if descriptorsLoaded {
		return nil
	}

	// 1. 缓存文件描述符
	for _, fileDesc := range internal.FdSet.GetFile() {
		fileDescCache[fileDesc.GetName()] = fileDesc
	}

	// 2. 缓存消息描述符（全限定名作为键）
	for _, fileDesc := range internal.FdSet.GetFile() {
		pkgName := fileDesc.GetPackage()
		for _, msgDesc := range fileDesc.GetMessageType() {
			fullName := getFullMessageName(pkgName, msgDesc.GetName())
			// 关键日志：打印消息的字段数
			log.Printf("缓存消息: %s (包名: %s, 字段数: %d)", fullName, pkgName, len(msgDesc.GetField()))
			messageDescCache[fullName] = msgDesc
		}
	}

	descriptorsLoaded = true
	return nil
}

// GenerateMergedTableSQL 核心：生成建表SQL
func GenerateMergedTableSQL(messageNames []string) error {
	// 确保描述符已加载
	if err := loadAllDescriptors(); err != nil {
		return err
	}

	sqlGenerator := pbmysql.NewPbMysqlDB()
	var mergedSQL strings.Builder

	for _, msgFullName := range messageNames {
		// 1. 获取消息描述符
		msgDesc, exists := messageDescCache[msgFullName]
		if !exists {
			log.Printf("警告: 未找到消息 %s 的描述符，跳过", msgFullName)
			continue
		}

		// 2. 关键修复：通过dynamicpb创建有效的proto.Message实例
		// 直接基于DescriptorProto生成，无需注册，兼容性100%
		msgInstance := dynamicpb.NewMessage(msgDesc.ProtoReflect().Descriptor())
		if msgInstance == nil {
			log.Printf("警告: 无法创建消息 %s 的实例，跳过", msgFullName)
			continue
		}
		log.Printf("已创建消息实例: %s", msgFullName)

		// 3. 验证实例有效性（可选，用于调试）
		verifyMessageValidity(msgFullName, msgInstance)

		// 4. 生成SQL（此时msgInstance是标准proto.Message类型）
		tableSQL := sqlGenerator.GetCreateTableSql(msgInstance)
		mergedSQL.WriteString(tableSQL)
		mergedSQL.WriteString("\n\n")
	}

	// 写入SQL文件
	if mergedSQL.Len() == 0 {
		log.Println("提示: 未生成任何SQL（无有效消息实例）")
		return nil
	}

	for _, protoDir := range config.ProtoDirs {
		if !util.HasGrpcService(protoDir) {
			continue
		}

		outputDir := util.BuildModelPath(protoDir)
		if err := os.MkdirAll(outputDir, 0755); err != nil {
			return err
		}

		sqlPath := outputDir + config.ModelPath + config.SqlExtension
		if err := os.WriteFile(sqlPath, []byte(mergedSQL.String()), 0644); err != nil {
			return err
		}
		log.Printf("SQL文件生成成功: %s", sqlPath)
	}

	return nil
}

// verifyMessageValidity 调试函数：验证消息实例是否包含字段
func verifyMessageValidity(msgName string, msg proto.Message) {
	msgReflect := msg.ProtoReflect()
	fieldCount := 0

	// 遍历消息的所有字段
	msgReflect.Range(func(fd protoreflect.FieldDescriptor, _ protoreflect.Value) bool {
		fieldCount++
		log.Printf("消息 %s 字段: %s (类型: %s, 编号: %d)",
			msgName, fd.Name(), fd.Kind(), fd.Number())
		return true
	})

	if fieldCount == 0 {
		log.Printf("警告: 消息 %s 无任何字段（请检查proto描述符是否完整）", msgName)
	}
}

// GenerateDBResource 入口函数：生成配置和SQL
func GenerateDBResource() {
	util.Wg.Add(1)
	go func() {
		defer util.Wg.Done()

		protoFile := config.DbTableName
		log.Printf("开始处理目标文件: %s", protoFile)

		// 提取消息名
		messageNames, err := extractMessageNamesFromProto(protoFile)
		if err != nil {
			log.Fatalf("提取消息名失败: %v", err)
		}
		if len(messageNames) == 0 {
			log.Fatalf("未从 %s 中提取到任何消息", protoFile)
		}

		// 并发执行：生成JSON配置
		util.Wg.Add(1)
		go func() {
			defer util.Wg.Done()
			if err := writeMessageNamesToJSON(messageNames); err != nil {
				log.Fatalf("生成JSON配置失败: %v", err)
			}
		}()

		// 并发执行：生成SQL
		util.Wg.Add(1)
		go func() {
			defer util.Wg.Done()
			if err := GenerateMergedTableSQL(messageNames); err != nil {
				log.Fatalf("生成SQL失败: %v", err)
			}
		}()
	}()
}
