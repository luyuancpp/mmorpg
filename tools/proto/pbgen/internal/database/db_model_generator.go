package database

import (
	"encoding/json"
	"fmt"
	pbmysql "github.com/luyuancpp/pbmysql-go"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
	"google.golang.org/protobuf/reflect/protoregistry"
	"google.golang.org/protobuf/types/descriptorpb"
	"log"
	"os"
	"pbgen/internal/config"
	"pbgen/util"
	"strings"
)

// MessageListConfig 定义消息名列表结构
type MessageListConfig struct {
	Messages []string `json:"messages"`
}

// extractMessageNamesFromProto 从指定proto文件中提取所有message名称
func extractMessageNamesFromProto(protoFile string) ([]string, error) {
	var messageNames []string

	// 读取all_in_one.pb.desc文件
	descData, err := os.ReadFile(config.AllInOneProtoDescFile)
	if err != nil {
		return nil, fmt.Errorf("读取描述文件失败: %v", err)
	}

	// 反序列化为FileDescriptorSet
	fdSet := &descriptorpb.FileDescriptorSet{}
	if err := proto.Unmarshal(descData, fdSet); err != nil {
		return nil, fmt.Errorf("反序列化描述文件失败: %v", err)
	}

	// 提取目标proto文件中的所有message名称
	for _, file := range fdSet.GetFile() {
		if !strings.Contains(file.GetName(), protoFile) {
			continue
		}
		for _, message := range file.GetMessageType() {
			messageNames = append(messageNames, message.GetName())
		}
	}

	return messageNames, nil
}

// writeMessageNamesToJSON 将消息名列表写入JSON配置文件
func writeMessageNamesToJSON(messages []string) error {
	configMessageJson := &MessageListConfig{Messages: messages}

	data, err := json.MarshalIndent(configMessageJson, "", "  ")
	if err != nil {
		return err
	}

	outputPath := config.TableGeneratorPath + config.DBTableMessageListJson
	if err := os.WriteFile(outputPath, data, 0644); err != nil {
		return err
	}

	log.Printf("配置文件已生成: %s\n", outputPath)
	return nil
}

// ConvertMessagesToSQLFiles 将Protobuf消息列表转换为SQL文件并写入指定目录
func ConvertMessagesToSQLFiles(messageNames []string) error {
	sqlGenerator := pbmysql.NewPbMysqlDB()

	createTableSQL := ""
	// 为每个消息生成SQL并写入文件
	for _, messageName := range messageNames {
		// 查找Protobuf消息类型
		msgType, err := protoregistry.GlobalTypes.FindMessageByName(protoreflect.FullName(messageName))
		if err != nil {
			log.Printf("警告: 未找到消息类型 %s, 跳过处理 - %v", messageName, err)
			continue
		}

		// 创建消息实例
		message := msgType.New().Interface()
		// 生成建表SQL语句
		createTableSQL += sqlGenerator.GetCreateTableSql(message)

	}

	// 遍历所有Proto目录
	for _, protoDir := range config.ProtoDirs {
		// 检查当前目录是否包含gRPC服务
		if !util.HasGrpcService(protoDir) {
			continue
		}

		// 构建输出目录路径
		outputDir := util.BuildModelPath(protoDir)
		// 创建输出目录（含多级目录）
		if err := os.MkdirAll(outputDir, 0755); err != nil {
			return err
		}

		// 构建SQL文件完整路径
		sqlFilePath := outputDir + config.ModelPath + config.SqlExtension
		// 写入SQL文件
		if err := os.WriteFile(sqlFilePath, []byte(createTableSQL), 0644); err != nil {
			return err
		}

		log.Printf("已生成SQL文件: %s", sqlFilePath)
	}

	return nil
}

// GenerateDBMessageListConfig 生成数据库相关的Protobuf消息列表配置
func GenerateDBMessageListConfig() {
	util.Wg.Add(1)

	go func() {
		defer util.Wg.Done()

		protoFile := config.DbTableName
		messageNames, err := extractMessageNamesFromProto(protoFile)
		if err != nil {
			log.Fatalf("提取消息名失败: %v", err)
		}

		util.Wg.Add(1)
		go func() {
			defer util.Wg.Done()
			if err := writeMessageNamesToJSON(messageNames); err != nil {
				log.Fatalf("写入JSON配置失败: %v", err)
			}
		}()

		util.Wg.Add(1)
		go func() {
			defer util.Wg.Done()
			if err := ConvertMessagesToSQLFiles(messageNames); err != nil {
				log.Fatalf("写入JSON配置失败: %v", err)
			}
		}()

	}()
}
