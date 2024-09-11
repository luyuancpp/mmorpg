package gen

import (
	"fmt"
	"log"
	"os"
	"path/filepath"

	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"
	"pbgen/config"
)

// 打印消息字段信息
func printMessageFields(descriptor *descriptorpb.DescriptorProto) {
	fmt.Printf("Message Type: %s\n", descriptor.GetName())
	for i, field := range descriptor.GetField() {
		fieldName := field.GetName()
		fieldType := field.GetType()
		fieldLabel := field.GetLabel()

		fmt.Printf("Field %d: %s (Type: %s, Label: %s)\n", i+1, fieldName, fieldType, fieldLabel)
	}
}

// 从 Descriptor Set 文件中读取消息结构
func CppPlayerDataLoadGenerator() {
	// 生成文件路径
	descFilePath := filepath.Join(
		config.DBDescDirectory,
		config.GameMysqlDBProtoFileName+config.ProtoDescExtension,
	)

	// 读取 Descriptor Set 文件
	data, err := os.ReadFile(descFilePath)
	if err != nil {
		log.Fatalf("Failed to read descriptor set file: %v", err)
	}

	// 解析 Descriptor Set 文件内容
	fdset := &descriptorpb.FileDescriptorSet{}
	if err := proto.Unmarshal(data, fdset); err != nil {
		log.Fatalf("Failed to unmarshal descriptor set: %v", err)
	}

	// 遍历文件描述符集合并打印消息字段
	for _, fileDesc := range fdset.GetFile() {
		for _, messageDesc := range fileDesc.GetMessageType() {
			printMessageFields(messageDesc)
		}
	}
}
