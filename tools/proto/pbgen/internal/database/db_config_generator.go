package database

import (
	"encoding/json"
	"fmt"
	"google.golang.org/protobuf/compiler/protogen"
	"google.golang.org/protobuf/types/pluginpb"
	"log"
	"os"
	"pbgen/internal/config"
	"pbgen/util"
)

// MessageListConfig 定义消息名列表结构
type MessageListConfig struct {
	Messages []string `json:"messages"`
}

// parseProtoFiles 解析指定的 proto 文件夹并提取所有 message 名字
func parseProtoFile(protoFile string) ([]string, error) {
	// 存储所有的 message 名称
	var messageNames []string

	// 使用 protogen.Options 解析指定的 proto 文件
	opts := protogen.Options{}

	// 初始化 CodeGeneratorRequest 请求
	req := &pluginpb.CodeGeneratorRequest{
		// 初始化 FileToGenerate 数组
	}

	// 收集 protoDir 中的所有 .proto 文件
	req.FileToGenerate = append(req.FileToGenerate, protoFile)

	// 确保至少有一个 proto 文件被找到
	if len(req.FileToGenerate) == 0 {
		return nil, fmt.Errorf("no .proto files found in the directory")
	}

	// 创建 protogen 插件
	_, err := opts.New(req)
	if err != nil {
		return nil, fmt.Errorf("failed to create plugin: %v", err)
	}

	opts.Run(func(gen *protogen.Plugin) error {
		// 遍历每个文件
		for _, file := range gen.Files {
			// 遍历文件中的所有消息类型
			for _, message := range file.Messages {
				// 将 message 名字加入到 messageNames 列表中
				messageNames = append(messageNames, string(message.Desc.Name()))
			}
		}
		// 返回 nil 表示没有错误
		return nil
	})

	if err != nil {
		return nil, fmt.Errorf("failed to run plugin: %v", err)
	}

	// 返回解析的 message 名称列表
	return messageNames, nil
}

// generateJSONConfig 生成 JSON 配置文件
func generateJSONConfig(protoFile string, messages []string) error {
	// 创建配置结构体
	configMessageJson := &MessageListConfig{Messages: messages}

	// 将配置转换为 JSON
	data, err := json.MarshalIndent(configMessageJson, "", "  ")
	if err != nil {
		return err
	}

	// 保存 JSON 到文件
	err = os.WriteFile(config.GeneratedOutputDirectory+config.DBTableMessageListJson, data, 0644)
	if err != nil {
		return err
	}

	fmt.Printf("配置文件已生成: %s\n", config.GeneratedOutputDirectory+config.DBTableMessageListJson)
	return nil
}

// GenerateDbProtoConfigFile 生成数据库配置的 proto 配置文件
func GenerateDbProtoConfigFile() {
	util.Wg.Add(1)

	go func() {
		// 提供要解析的 proto 文件路径
		// 解析 proto 文件并获取消息名字列表
		defer util.Wg.Done()

		protoFile := config.ProtoDirs[config.DbProtoDirIndex] + config.DbTableName

		// 获取 message 名字列表
		messageNames, err := parseProtoFile(protoFile)
		if err != nil {
			log.Fatalf("解析 proto 文件时出错: %v", err)
		}

		// 生成 JSON 配置文件
		if err := generateJSONConfig(protoFile, messageNames); err != nil {
			log.Fatalf("生成 JSON 配置时出错: %v", err)
		}
	}()
}
