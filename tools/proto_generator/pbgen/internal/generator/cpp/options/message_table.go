package options

import (
	"github.com/luyuancpp/protooption"
	"go.uber.org/zap"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"
	"pbgen/internal/prototools/option"
	"pbgen/logger" // 引入全局logger包
)

// BuildOption 注册消息选项和扩展字段的回调函数
func BuildOption() {
	// 直接使用全局logger，无需单独初始化
	if logger.Global == nil {
		// 防御性检查：如果全局logger未初始化，创建临时logger
		tempLogger, _ := zap.NewProduction()
		defer tempLogger.Sync()
		tempLogger.Warn("全局logger未初始化，使用临时logger")

		registerCallbacks(tempLogger)
		return
	}

	registerCallbacks(logger.Global)
}

// registerCallbacks 提取回调注册逻辑，便于复用
func registerCallbacks(log *zap.Logger) {
	prototools.RegisterOptionCallback(prototools.OptionTypeMessage,
		func(desc interface{}, opts interface{}) error {
			// 这里 desc 是 *descriptorpb.DescriptorProto
			msg := desc.(*descriptorpb.DescriptorProto)

			value := proto.GetExtension(
				opts.(*descriptorpb.MessageOptions),
				messageoption.E_OptionTableName,
			)

			if value != nil {
				log.Info("[CPP] Message table name",
					zap.String("message_name", msg.GetName()),
					zap.Any("table_name", value),
				)
			}

			return nil
		})

	prototools.RegisterExtensionCallback(
		messageoption.E_OptionTableName,
		func(desc interface{}, value interface{}) error {
			msg := desc.(*descriptorpb.DescriptorProto)
			log.Info("[Ext] Message table name",
				zap.String("message_name", msg.GetName()),
				zap.Any("table_name", value),
			)
			return nil
		})
}
