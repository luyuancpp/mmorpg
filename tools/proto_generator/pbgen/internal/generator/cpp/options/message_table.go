package options

import (
	"fmt"
	"github.com/iancoleman/strcase"
	"github.com/luyuancpp/protooption"
	"go.uber.org/zap"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"
	"os"
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

	// 注册 Message 类型的 Option 回调，用于生成属性同步的 C++ 代码和头文件
	prototools.RegisterOptionCallback(prototools.OptionTypeMessage,
		func(desc interface{}, opts interface{}) error {
			// 定义模板渲染所需的结构体
			type AttributeField struct {
				FieldName      string // 原始字段名
				CamelFieldName string // 驼峰式字段名
				Number         int32  // 字段编号
			}

			type AttributeSyncMessage struct {
				MessageName string           // 消息名称
				CppClass    string           // C++ 类名
				Fields      []AttributeField // 消息字段列表
			}

			// 类型断言：确保 desc 是 DescriptorProto 类型
			msg, ok := desc.(*descriptorpb.DescriptorProto)
			if !ok {
				return fmt.Errorf("desc 类型断言失败，期望 *descriptorpb.DescriptorProto，实际为 %T", desc)
			}

			// 获取自定义 Option：是否启用属性同步生成
			rawValue := proto.GetExtension(
				opts.(*descriptorpb.MessageOptions),
				messageoption.E_OptionAttributeSync,
			)
			if rawValue == nil {
				return nil // 未配置该 Option，直接返回
			}

			// 验证 Option 值为 true 才执行生成逻辑
			enabled, ok := rawValue.(bool)
			if !ok || !enabled {
				return nil
			}

			// 构建模板渲染的数据源
			asm := AttributeSyncMessage{
				MessageName: msg.GetName(),
				CppClass:    msg.GetName(),
				Fields:      make([]AttributeField, 0, len(msg.GetField())),
			}

			// 遍历消息字段，填充字段信息
			for _, field := range msg.GetField() {
				asm.Fields = append(asm.Fields, AttributeField{
					FieldName:      field.GetName(),
					CamelFieldName: strcase.ToCamel(field.GetName()),
					Number:         field.GetNumber(),
				})
			}

			logger.Global.Info("开始生成属性同步代码",
				zap.String("message_name", asm.MessageName),
				zap.Int("field_count", len(asm.Fields)),
			)

			// 基础文件名（小写）
			base := strings.ToLower(asm.MessageName)
			// 目标目录（确保目录存在）
			outputDir := _config.Global.Paths.RoomAttributeSyncDir
			if err := os.MkdirAll(outputDir, 0755); err != nil {
				return fmt.Errorf("创建输出目录失败: %w", err)
			}

			// 定义模板函数映射
			funcMap := template.FuncMap{
				"ToLower": strings.ToLower,
				"ToCamel": strcase.ToCamel,
			}

			// ========== 生成 C++ 源文件 (.cpp) ==========
			cppFile, err := _config.Global.GetOutputPath("attribute_sync_cpp", outputDir, base)
			if err != nil {
				return fmt.Errorf("获取 CPP 文件路径失败: %w", err)
			}
			cppTemplatePath, err := _config.Global.GetTemplatePath("attribute_sync_cpp")
			if err != nil {
				return fmt.Errorf("获取 CPP 模板路径失败: %w", err)
			}
			if err := utils.RenderTemplateToFileWithFuncs(cppTemplatePath, cppFile, asm, funcMap); err != nil {
				return fmt.Errorf("生成 CPP 文件失败: %w", err)
			}

			// ========== 生成 C++ 头文件 (.h) ==========
			hFile, err := _config.Global.GetOutputPath("attribute_sync_h", outputDir, base)
			if err != nil {
				return fmt.Errorf("获取头文件路径失败: %w", err)
			}
			hTemplatePath, err := _config.Global.GetTemplatePath("attribute_sync_h")
			if err != nil {
				return fmt.Errorf("获取头文件模板路径失败: %w", err)
			}
			if err := utils.RenderTemplateToFileWithFuncs(hTemplatePath, hFile, asm, funcMap); err != nil {
				return fmt.Errorf("生成头文件失败: %w", err)
			}

			logger.Global.Info("属性同步代码生成完成",
				zap.String("message_name", asm.MessageName),
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
