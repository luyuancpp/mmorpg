package options

import (
	"fmt"
	messageoption "github.com/luyuancpp/protooption"
	"go.uber.org/zap"
	"google.golang.org/protobuf/types/descriptorpb"
	"protogen/internal/prototools/option"
	"protogen/logger"
)

func BuildOption() {
	prototools.RegisterExtensionCallback(
		messageoption.E_OptionTableName,
		func(desc interface{}, value interface{}) error {
			msg := desc.(*descriptorpb.DescriptorProto)

			logger.Global.Info("读取消息表名Extension",
				zap.String("message_name", msg.GetName()),
				zap.String("table_name", fmt.Sprint(value)), // 兼容任意类型的value输出
				zap.String("source", "ExtensionCallback"),
			)
			return nil
		})
}
