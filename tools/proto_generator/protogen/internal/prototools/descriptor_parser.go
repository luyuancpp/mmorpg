package prototools

import (
	"os"
	"strings"

	"protogen/internal"
	_config "protogen/internal/config"
	"protogen/logger"

	"go.uber.org/zap"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"
)

// parseDescriptorFile parses the generated descriptor file.
func parseDescriptorFile() error {
	data, err := os.ReadFile(_config.Global.Paths.AllInOneDesc)
	if err != nil {
		logger.Global.Fatal("Failed to read descriptor file",
			zap.String("file_path", _config.Global.Paths.AllInOneDesc),
			zap.Error(err),
		)
	}
	logger.Global.Info("Descriptor generation: file read successfully",
		zap.String("file_path", _config.Global.Paths.AllInOneDesc),
		zap.Int("file_size_bytes", len(data)),
	)

	if internal.FdSet == nil {
		internal.FdSet = &descriptorpb.FileDescriptorSet{}
	}
	if err := proto.Unmarshal(data, internal.FdSet); err != nil {
		logger.Global.Fatal("Failed to unmarshal descriptor file",
			zap.String("file_path", _config.Global.Paths.AllInOneDesc),
			zap.Error(err),
			zap.String("hint", "file may be corrupted or version-incompatible"),
		)
	}

	fileCount := len(internal.FdSet.GetFile())
	logger.Global.Info("Descriptor generation: file parsed successfully",
		zap.Int("file_descriptor_count", fileCount),
	)

	for _, fileDesc := range internal.FdSet.GetFile() {
		fileName := fileDesc.GetName()
		if strings.Contains(fileName, "proto/service/go/grpc") ||
			strings.Contains(fileName, "proto/common") {
			msgTypeCount := len(fileDesc.GetMessageType())
			logger.Global.Debug("Descriptor file details",
				zap.String("file_name", fileName),
				zap.Int("message_type_count", msgTypeCount),
			)
		}
	}

	return nil
}
