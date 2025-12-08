package options

import (
	"fmt"
	"github.com/iancoleman/strcase"
	"github.com/luyuancpp/protooption"
	"go.uber.org/zap"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"
	"os"
	"path/filepath"
	_config "pbgen/internal/config"
	"pbgen/internal/prototools/option"
	"pbgen/internal/tpl"
	"pbgen/logger" // 引入全局logger包
	"strings"
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

	prototools.RegisterOptionCallback(prototools.OptionTypeMessage,
		func(desc interface{}, opts interface{}) error {

			// ==== 定义字段结构 ====
			type AttributeField struct {
				FieldName      string
				CamelFieldName string
				Number         int32
				CppComponent   string
			}

			type AttributeSyncMessage struct {
				MessageName string
				CppClass    string
				Fields      []AttributeField
			}

			// ==== 获取 message 描述符 ====
			msg, ok := desc.(*descriptorpb.DescriptorProto)
			if !ok {
				return fmt.Errorf("desc 类型断言失败")
			}

			// ==== 读取 option ====
			rawValue := proto.GetExtension(
				opts.(*descriptorpb.MessageOptions),
				messageoption.E_OptionAttributeSync,
			)

			// 未设置 option → 不处理
			if rawValue == nil {
				return nil
			}

			// option_attribute_sync = false → 不处理
			enabled, ok := rawValue.(bool)
			if !ok || !enabled {
				return nil
			}

			// ==== 构造 AttributeSyncMessage ====
			asm := AttributeSyncMessage{
				MessageName: msg.GetName(),
				CppClass:    msg.GetName(),
				Fields:      []AttributeField{},
			}

			// ==== 收集字段 ====
			for _, field := range msg.GetField() {
				asm.Fields = append(asm.Fields, AttributeField{
					FieldName:      field.GetName(),
					CamelFieldName: strcase.ToCamel(field.GetName()),
					Number:         field.GetNumber(),
				})
			}

			// ==== 打印日志 ====
			log.Info("Generating attribute sync",
				zap.String("message_name", asm.MessageName),
				zap.Int("field_count", len(asm.Fields)),
			)

			// ============================
			// 🔥🔥 直接生成文件
			// ============================

			// 生成路径：scene/<xxx>/attribute_sync/
			outDir := filepath.Join(_config.Global.Paths.SceneAttributeSyncDir, "attribute_sync", strings.ToLower(asm.MessageName))

			if err := os.MkdirAll(outDir, 0755); err != nil {
				return fmt.Errorf("创建目录失败: %w", err)
			}

			cppFile := filepath.Join(outDir, asm.MessageName+"_attribute_sync.cpp")
			hFile := filepath.Join(outDir, asm.MessageName+"_attribute_sync.h")

			if err := tpl.ExecuteTemplateToFile("attribute_sync.cpp.tmpl", cppFile, asm); err != nil {
				return fmt.Errorf("生成 CPP 文件失败: %w", err)
			}
			if err := tpl.ExecuteTemplateToFile("attribute_sync.h.tmpl", hFile, asm); err != nil {
				return fmt.Errorf("生成 H 文件失败: %w", err)
			}

			log.Info("Attribute sync files generated",
				zap.String("cpp_file", cppFile),
				zap.String("h_file", hFile),
			)

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
