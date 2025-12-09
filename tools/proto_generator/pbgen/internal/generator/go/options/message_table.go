package options

import (
	"fmt"
	messageoption "github.com/luyuancpp/protooption"
	"go.uber.org/zap" // 引入zap用于结构化日志字段
	"google.golang.org/protobuf/types/descriptorpb"
	"pbgen/internal/prototools/option"
	"pbgen/logger" // 引入全局logger包
)

func BuildOption() {
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
