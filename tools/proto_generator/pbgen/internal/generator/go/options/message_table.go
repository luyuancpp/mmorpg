package options

import (
	"fmt"
	messageoption "github.com/luyuancpp/protooption"
	"go.uber.org/zap" // 引入zap用于结构化日志字段
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"
	"pbgen/internal/prototools/option"
	"pbgen/logger" // 引入全局logger包
)

func BuildOption() {
	// 注册Message类型的Option回调
	prototools.RegisterOptionCallback(prototools.OptionTypeMessage,
		func(desc interface{}, opts interface{}) error {
			// 类型断言转换为消息描述符
			msg := desc.(*descriptorpb.DescriptorProto)

			// 读取自定义OptionTableName扩展字段
			value := proto.GetExtension(
				opts.(*descriptorpb.MessageOptions),
				messageoption.E_OptionTableName,
			)

			if value != nil {
				// 替换fmt.Println为结构化日志
				logger.Global.Info("读取消息表名Option",
					zap.String("message_name", msg.GetName()),
					zap.String("table_name", fmt.Sprint(value)), // 兼容任意类型的value输出
					zap.String("source", "OptionCallback"),
				)
			}

			return nil
		})

	// 注册Extension回调
	prototools.RegisterExtensionCallback(
		messageoption.E_OptionTableName,
		func(desc interface{}, value interface{}) error {
			// 类型断言转换为消息描述符
			msg := desc.(*descriptorpb.DescriptorProto)

			// 替换fmt.Println为结构化日志
			logger.Global.Info("读取消息表名Extension",
				zap.String("message_name", msg.GetName()),
				zap.String("table_name", fmt.Sprint(value)), // 兼容任意类型的value输出
				zap.String("source", "ExtensionCallback"),
			)
			return nil
		})
}
