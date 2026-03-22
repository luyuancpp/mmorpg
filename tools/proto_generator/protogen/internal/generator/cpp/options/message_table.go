package options

import (
	"fmt"
	"github.com/iancoleman/strcase"
	"github.com/luyuancpp/protooption"
	"go.uber.org/zap"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"
	"os"
	_config "protogen/internal/config"
	"protogen/internal/prototools/option"
	"protogen/internal/utils"
	"protogen/logger"
	"strings"
	"text/template"
)

// BuildOption 注册消息选项和扩展字段的回调函数
func BuildOption() {
	if logger.Global == nil {
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
	prototools.RegisterCallback(prototools.OptionTypeMessage,
		func(desc interface{}, opts interface{}, context *prototools.OptionContext) error {
			type AttributeField struct {
				FieldName      string // 原始字段名（如 entity_id）
				CamelFieldName string // 大驼峰字段名（如 EntityId）
				Number         int32  // 字段编号
				FieldType      string // 字段类型名称（如 uint64、Transform、CombatStateFlagsPbComponent）
			}

			type AttributeSyncMessage struct {
				MessageName     string           // 消息名称（如 ActorBaseAttributesS2C）
				CppClass        string           // C++类名（与消息名一致）
				ProtoHeaderFile string           // proto文件对应的头文件
				ProtoFileName   string           // 原始proto文件名
				Fields          []AttributeField // 消息所有字段
			}

			msgDesc, ok := desc.(*descriptorpb.DescriptorProto)
			if !ok {
				return fmt.Errorf("desc类型断言失败，期望*descriptorpb.DescriptorProto，实际为%T", desc)
			}

			if context == nil || context.File == nil {
				return fmt.Errorf("获取消息[%s]的文件上下文失败", msgDesc.GetName())
			}
			fileDesc := context.File

			protoFileName := fileDesc.GetName()
			if protoFileName == "" {
				return fmt.Errorf("消息[%s]所属的proto文件名为空", msgDesc.GetName())
			}
			protoHeaderFile := strings.TrimSuffix(protoFileName, ".proto") + ".pb.h"

			rawValue := proto.GetExtension(
				opts.(*descriptorpb.MessageOptions),
				messageoption.E_OptionAttributeSync,
			)
			if rawValue == nil {
				logger.Global.Debug("消息未配置属性同步Option，跳过生成",
					zap.String("message_name", msgDesc.GetName()),
					zap.String("proto_file", protoFileName),
				)
				return nil
			}

			enabled, ok := rawValue.(bool)
			if !ok {
				return fmt.Errorf("消息[%s]的属性同步Option类型错误，期望bool，实际为%T",
					msgDesc.GetName(), rawValue)
			}
			if !enabled {
				logger.Global.Debug("消息属性同步Option为false，跳过生成",
					zap.String("message_name", msgDesc.GetName()),
					zap.String("proto_file", protoFileName),
				)
				return nil
			}

			asm := AttributeSyncMessage{
				MessageName:     msgDesc.GetName(),
				CppClass:        msgDesc.GetName(),
				ProtoFileName:   protoFileName,
				ProtoHeaderFile: protoHeaderFile,
				Fields:          make([]AttributeField, 0, len(msgDesc.GetField())),
			}

			for _, field := range msgDesc.GetField() {
				fieldType := getShortFieldTypeName(fileDesc, msgDesc, field)

				asm.Fields = append(asm.Fields, AttributeField{
					FieldName:      field.GetName(),
					CamelFieldName: strcase.ToCamel(field.GetName()),
					Number:         field.GetNumber(),
					FieldType:      fieldType, // 现在能拿到CombatStateFlagsPbComponent这类短名称
				})
			}

			logger.Global.Info("开始生成属性同步代码",
				zap.String("message_name", asm.MessageName),
				zap.String("proto_file", asm.ProtoFileName),
				zap.String("proto_header", asm.ProtoHeaderFile),
				zap.Int("field_count", len(asm.Fields)),
			)

			base := strings.ToLower(msgDesc.GetName())
			outputDir := _config.Global.Paths.SceneAttributeSyncDir
			if err := os.MkdirAll(outputDir, 0755); err != nil {
				return fmt.Errorf("创建输出目录[%s]失败: %w", outputDir, err)
			}

			funcMap := template.FuncMap{
				"ToLower": strings.ToLower,
				"ToCamel": strcase.ToCamel,
			}
			cppFile, err := _config.Global.GetOutputPath("attribute_sync_cpp", outputDir, base)
			if err != nil {
				return fmt.Errorf("获取CPP文件路径失败: %w", err)
			}
			cppTemplatePath, err := _config.Global.GetTemplatePath("attribute_sync_cpp")
			if err != nil {
				return fmt.Errorf("获取CPP模板路径失败: %w", err)
			}
			if err := utils.RenderTemplateToFileWithFuncs(cppTemplatePath, cppFile, asm, funcMap); err != nil {
				return fmt.Errorf("生成CPP文件[%s]失败: %w", cppFile, err)
			}
			hFile, err := _config.Global.GetOutputPath("attribute_sync_h", outputDir, base)
			if err != nil {
				return fmt.Errorf("获取头文件路径失败: %w", err)
			}
			hTemplatePath, err := _config.Global.GetTemplatePath("attribute_sync_h")
			if err != nil {
				return fmt.Errorf("获取头文件模板路径失败: %w", err)
			}
			if err := utils.RenderTemplateToFileWithFuncs(hTemplatePath, hFile, asm, funcMap); err != nil {
				return fmt.Errorf("生成头文件[%s]失败: %w", hFile, err)
			}

			logger.Global.Info("属性同步代码生成完成",
				zap.String("message_name", asm.MessageName),
				zap.String("proto_header", asm.ProtoHeaderFile),
				zap.String("cpp_file", cppFile),
				zap.String("h_file", hFile),
			)

			return nil
		})
}

// getShortFieldTypeName 解析Protobuf字段的**短类型名**（核心优化）
// - 基础类型：uint64、string、bool
// - 自定义消息：Transform、Velocity、CombatStateFlagsPbComponent
// - 官方消息：Timestamp（而非google.protobuf.Timestamp）
func getShortFieldTypeName(fileDesc *descriptorpb.FileDescriptorProto, msgDesc *descriptorpb.DescriptorProto, field *descriptorpb.FieldDescriptorProto) string {
	fieldType := field.GetType()
	if fieldType != descriptorpb.FieldDescriptorProto_TYPE_MESSAGE &&
		fieldType != descriptorpb.FieldDescriptorProto_TYPE_ENUM {
		return strings.ToLower(fieldType.String()[len("TYPE_"):])
	}

	typeName := field.GetTypeName()
	if strings.HasPrefix(typeName, ".") {
		typeName = typeName[1:]
	}
	parts := strings.Split(typeName, ".")
	shortName := parts[len(parts)-1]

	// （可选：确保自定义类型名100%准确）
	return shortName
}
