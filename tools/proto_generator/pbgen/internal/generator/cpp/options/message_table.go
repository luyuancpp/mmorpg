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
	"pbgen/internal/utils"
	"pbgen/logger" // 引入全局logger包
	"strings"
	"text/template"
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

			type AttributeField struct {
				FieldName      string
				CamelFieldName string
				Number         int32
			}

			type AttributeSyncMessage struct {
				MessageName string
				CppClass    string
				Fields      []AttributeField
			}

			msg, ok := desc.(*descriptorpb.DescriptorProto)
			if !ok {
				return fmt.Errorf("desc 类型断言失败")
			}

			rawValue := proto.GetExtension(
				opts.(*descriptorpb.MessageOptions),
				messageoption.E_OptionAttributeSync,
			)
			if rawValue == nil {
				return nil
			}
			enabled, ok := rawValue.(bool)
			if !ok || !enabled {
				return nil
			}

			asm := AttributeSyncMessage{
				MessageName: msg.GetName(),
				CppClass:    msg.GetName(),
				Fields:      make([]AttributeField, 0, len(msg.GetField())),
			}

			for _, field := range msg.GetField() {
				asm.Fields = append(asm.Fields, AttributeField{
					FieldName:      field.GetName(),
					CamelFieldName: strcase.ToCamel(field.GetName()),
					Number:         field.GetNumber(),
				})
			}

			logger.Global.Info("Generating attribute sync",
				zap.String("message_name", asm.MessageName),
				zap.Int("field_count", len(asm.Fields)),
			)

			base := strings.ToLower(asm.MessageName)

			cppPath, err := _config.Global.GetOutputPath("attribute_sync_cpp", _config.Global.Paths.RoomAttributeSyncDir, base)
			if err != nil {
				logger.Global.Fatal("创建目录失败", zap.Error(err))
			}

			outDir := filepath.Join(cppPath,
				"attribute_sync", strings.ToLower(asm.MessageName))
			if err := os.MkdirAll(outDir, 0755); err != nil {
				logger.Global.Fatal("创建目录失败", zap.String("dir", outDir), zap.Error(err))
			}

			cppFile := filepath.Join(outDir, base+"_attribute_sync.cpp")
			hFile := filepath.Join(outDir, base+"_attribute_sync.h")

			// 获取 cpp 模板
			cppTemplatePath, err := _config.Global.GetTemplatePath("attribute_sync_cpp")
			if err != nil {
				logger.Global.Fatal("读取 template 文件失败",
					zap.String("template_name", "attribute_sync_cpp"),
					zap.Error(err))
			}

			funcMap := template.FuncMap{
				"ToLower": strings.ToLower,
				"ToCamel": strcase.ToCamel,
			}

			// 生成 cpp
			if err := utils.RenderTemplateToFileWithFuncs(
				cppTemplatePath, cppFile, asm, funcMap); err != nil {
				logger.Global.Fatal("生成 CPP 文件失败",
					zap.String("file", cppFile), zap.Error(err))
			}

			logger.Global.Info("Attribute sync files generated",
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
