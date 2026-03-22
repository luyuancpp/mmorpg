package prototools

import (
	"os"
	"strings"

	"go.uber.org/zap"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"
	"protogen/internal"
	_config "protogen/internal/config"
	"protogen/logger"
)

// parseDescriptorFile 解析生成的描述符文件
func parseDescriptorFile() error {
	data, err := os.ReadFile(_config.Global.Paths.AllInOneDesc)
	if err != nil {
		logger.Global.Fatal("读取描述符文件失败",
			zap.String("file_path", _config.Global.Paths.AllInOneDesc),
			zap.Error(err),
		)
	}
	logger.Global.Info("描述符生成: 读取文件成功",
		zap.String("file_path", _config.Global.Paths.AllInOneDesc),
		zap.Int("file_size_bytes", len(data)),
	)

	if internal.FdSet == nil {
		internal.FdSet = &descriptorpb.FileDescriptorSet{}
	}
	if err := proto.Unmarshal(data, internal.FdSet); err != nil {
		logger.Global.Fatal("描述符文件反序列化失败",
			zap.String("file_path", _config.Global.Paths.AllInOneDesc),
			zap.Error(err),
			zap.String("hint", "可能是文件损坏或版本不兼容"),
		)
	}

	fileCount := len(internal.FdSet.GetFile())
	logger.Global.Info("描述符生成: 成功解析文件",
		zap.Int("file_descriptor_count", fileCount),
	)

	for _, fileDesc := range internal.FdSet.GetFile() {
		fileName := fileDesc.GetName()
		if strings.Contains(fileName, "proto/service/go/grpc") ||
			strings.Contains(fileName, "proto/common") {
			msgTypeCount := len(fileDesc.GetMessageType())
			logger.Global.Debug("描述符文件详情",
				zap.String("file_name", fileName),
				zap.Int("message_type_count", msgTypeCount),
			)
		}
	}

	return nil
}
