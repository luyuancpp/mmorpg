package database

import (
	"encoding/json"
	"fmt"
	"google.golang.org/protobuf/proto"
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

// parseProtoFile 解析指定的 proto 文件夹并提取所有 message 名字
func parseProtoFile(protoFile string) ([]string, error) {
	// 存储所有的 message 名称
	var messageNames []string

	// 读取 all_in_one.pb.desc 文件
	descData, err := os.ReadFile(config.AllInOneProtoDescFile)
	if err != nil {
		return nil, fmt.Errorf("failed to read descriptor file: %v", err)
	}

	// 反序列化 descriptor 文件为 FileDescriptorSet
	fdSet := &descriptorpb.FileDescriptorSet{}
	if err := proto.Unmarshal(descData, fdSet); err != nil {
		return nil, fmt.Errorf("failed to unmarshal descriptor file: %v", err)
	}

	// 定义一个处理函数，遍历所有的文件并提取 message 名称
	// 遍历每个文件
	for _, file := range fdSet.GetFile() {
		if !strings.Contains(file.GetName(), protoFile) {
			continue
		}
		// 遍历文件中的所有消息类型
		for _, message := range file.GetMessageType() {
			// 将 message 名字加入到 messageNames 列表中
			messageNames = append(messageNames, message.GetName())
		}
	}
	// 返回 nil 表示没有错误

	// 返回解析的 message 名称列表
	return messageNames, nil
}

// generateJSONConfig 生成 JSON 配置文件
func generateJSONConfig(messages []string) error {
	// 创建配置结构体
	configMessageJson := &MessageListConfig{Messages: messages}

	// 将配置转换为 JSON
	data, err := json.MarshalIndent(configMessageJson, "", "  ")
	if err != nil {
		return err
	}

	// 保存 JSON 到文件
	err = os.WriteFile(config.TableGeneratorPath+config.DBTableMessageListJson, data, 0644)
	if err != nil {
		return err
	}

	log.Printf("配置文件已生成: %s\n", config.TableGeneratorPath+config.DBTableMessageListJson)
	return nil
}

// GenerateDbProtoConfigFile 生成数据库配置的 proto 配置文件
func GenerateDbProtoConfigFile() {
	util.Wg.Add(1)

	go func() {
		// 提供要解析的 proto 文件路径
		// 解析 proto 文件并获取消息名字列表
		defer util.Wg.Done()

		protoFile := config.DbTableName

		// 获取 message 名字列表
		messageNames, err := parseProtoFile(protoFile)
		if err != nil {
			log.Fatalf("解析 proto 文件时出错: %v", err)
		}

		// 生成 JSON 配置文件
		if err := generateJSONConfig(messageNames); err != nil {
			log.Fatalf("生成 JSON 配置时出错: %v", err)
		}
	}()
}
