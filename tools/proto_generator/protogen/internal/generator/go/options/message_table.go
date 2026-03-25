package options

import (
	"fmt"
	prototools "protogen/internal/prototools/option"
	"protogen/logger"

	messageoption "github.com/luyuancpp/protooption"
	"go.uber.org/zap"
	"google.golang.org/protobuf/types/descriptorpb"
)

func BuildOption() {
	prototools.RegisterExtensionCallback(
		messageoption.E_OptionTableName,
		func(desc interface{}, value interface{}) error {
			msg := desc.(*descriptorpb.DescriptorProto)

			logger.Global.Info("Read message table name extension",
				zap.String("message_name", msg.GetName()),
				zap.String("table_name", fmt.Sprint(value)), // handle any value type
				zap.String("source", "ExtensionCallback"),
			)
			return nil
		})
}
