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
	prototools.RegisterCallback(prototools.OptionTypeMessage,
		func(desc interface{}, opts interface{}, context *prototools.OptionContext) error {
			// 定义模板渲染所需的结构体
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

			// 第一步：类型断言获取Message描述符
			msgDesc, ok := desc.(*descriptorpb.DescriptorProto)
			if !ok {
				return fmt.Errorf("desc类型断言失败，期望*descriptorpb.DescriptorProto，实际为%T", desc)
			}

			// 第二步：从上下文获取准确的FileDescriptorProto
			if context == nil || context.File == nil {
				return fmt.Errorf("获取消息[%s]的文件上下文失败", msgDesc.GetName())
			}
			fileDesc := context.File

			// 第三步：生成正确的proto头文件路径
			protoFileName := fileDesc.GetName()
			if protoFileName == "" {
				return fmt.Errorf("消息[%s]所属的proto文件名为空", msgDesc.GetName())
			}
			protoHeaderFile := strings.TrimSuffix(protoFileName, ".proto") + ".pb.h"

			// 获取自定义Option：是否启用属性同步生成
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

			// 验证Option值为true才执行生成
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

			// 构建模板渲染的数据源
			asm := AttributeSyncMessage{
				MessageName:     msgDesc.GetName(),
				CppClass:        msgDesc.GetName(),
				ProtoFileName:   protoFileName,
				ProtoHeaderFile: protoHeaderFile,
				Fields:          make([]AttributeField, 0, len(msgDesc.GetField())),
			}

			// 遍历消息字段，填充字段信息（核心：解析自定义类型短名称）
			for _, field := range msgDesc.GetField() {
				// 解析字段类型名称（重点：提取短名称）
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

			// 基础文件名（小写消息名）
			base := strings.ToLower(msgDesc.GetName())
			// 输出目录（确保存在）
			outputDir := _config.Global.Paths.RoomAttributeSyncDir
			if err := os.MkdirAll(outputDir, 0755); err != nil {
				return fmt.Errorf("创建输出目录[%s]失败: %w", outputDir, err)
			}

			// 模板函数映射
			funcMap := template.FuncMap{
				"ToLower": strings.ToLower,
				"ToCamel": strcase.ToCamel,
			}

			// ========== 1. 生成C++源文件 (.cpp) ==========
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

			// ========== 2. 生成C++头文件 (.h) ==========
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
// 输出示例：
// - 基础类型：uint64、string、bool
// - 自定义消息：Transform、Velocity、CombatStateFlagsPbComponent
// - 官方消息：Timestamp（而非google.protobuf.Timestamp）
func getShortFieldTypeName(fileDesc *descriptorpb.FileDescriptorProto, msgDesc *descriptorpb.DescriptorProto, field *descriptorpb.FieldDescriptorProto) string {
	// 1. 处理基础类型（如 uint64、string、bool 等）
	fieldType := field.GetType()
	if fieldType != descriptorpb.FieldDescriptorProto_TYPE_MESSAGE &&
		fieldType != descriptorpb.FieldDescriptorProto_TYPE_ENUM {
		// 直接返回基础类型短名称（TYPE_UINT64 → uint64）
		return strings.ToLower(fieldType.String()[len("TYPE_"):])
	}

	// 2. 处理消息/枚举类型（提取短名称）
	typeName := field.GetTypeName()
	// 第一步：去掉前缀的 "."（如 .com.game.CombatStateFlagsPbComponent → com.game.CombatStateFlagsPbComponent）
	if strings.HasPrefix(typeName, ".") {
		typeName = typeName[1:]
	}
	// 第二步：按 "." 分割，取最后一段（即短名称）
	parts := strings.Split(typeName, ".")
	shortName := parts[len(parts)-1]

	// 3. 特殊处理：如果是当前文件内的类型，直接返回短名称
	// （可选：确保自定义类型名100%准确）
	return shortName
}
