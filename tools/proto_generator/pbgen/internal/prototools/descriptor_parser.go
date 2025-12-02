package prototools

import (
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"
	"log"
	"os"
	"pbgen/internal"
	_config "pbgen/internal/config"
	"strings"
)

// parseDescriptorFile 解析生成的描述符文件
func parseDescriptorFile() error {
	// 读取文件内容
	data, err := os.ReadFile(_config.Global.Paths.AllInOneDesc)
	if err != nil {
		log.Fatalf("读取文件失败: %w", err)
	}
	log.Printf("描述符生成: 读取文件成功，大小=%d字节", len(data))

	// 解析为FileDescriptorSet
	if internal.FdSet == nil {
		internal.FdSet = &descriptorpb.FileDescriptorSet{}
	}
	if err := proto.Unmarshal(data, internal.FdSet); err != nil {
		log.Fatalf("反序列化失败: %w，可能是文件损坏或版本不兼容", err)
	}

	// 验证解析结果
	log.Printf("描述符生成: 成功解析，包含%d个文件描述符", len(internal.FdSet.GetFile()))
	for _, fileDesc := range internal.FdSet.GetFile() {
		// 打印关键文件信息
		if strings.Contains(fileDesc.GetName(), "proto/service/go/grpc") ||
			strings.Contains(fileDesc.GetName(), "proto/common") {
			log.Printf("  描述符文件: %s，包含%d个消息类型",
				fileDesc.GetName(), len(fileDesc.GetMessageType()))
		}
	}

	return nil
}
