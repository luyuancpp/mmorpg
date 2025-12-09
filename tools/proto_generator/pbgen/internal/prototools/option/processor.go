package prototools

import (
	"fmt"
	"sync"

	"go.uber.org/zap"
	"google.golang.org/protobuf/types/descriptorpb"
	"pbgen/logger"
)

// OptionType 定义Option类型枚举
type OptionType int

const (
	OptionTypeFile OptionType = iota
	OptionTypeService
	OptionTypeMethod
	OptionTypeMessage
	OptionTypeField
)

// OptionCallback 定义回调函数类型，新增context参数
type OptionCallback func(desc interface{}, opts interface{}, context *OptionContext) error

// OptionContext 封装各层级元素的归属上下文
type OptionContext struct {
	File    *descriptorpb.FileDescriptorProto    // 所属文件（所有类型都有）
	Service *descriptorpb.ServiceDescriptorProto // 所属Service（仅Method有）
	Message *descriptorpb.DescriptorProto        // 所属Message（仅Field、嵌套Message有）
}

// 回调函数注册表
var (
	callbacksMap = make(map[OptionType][]OptionCallback)
	mu           sync.RWMutex
)

// getCallbacks 获取指定类型的回调函数
func getCallbacks(t OptionType) []OptionCallback {
	mu.RLock()
	defer mu.RUnlock()
	return callbacksMap[t]
}

// ProcessAllOptions 处理所有Option，入口函数
func ProcessAllOptions(wg *sync.WaitGroup, fdSet *descriptorpb.FileDescriptorSet) error {
	for _, f := range fdSet.File {
		// 文件级上下文：只有File，其他为nil
		context := &OptionContext{File: f}
		processFile(f, wg, context)
	}
	return nil
}

// processFile 处理文件级Option，并递归处理内部元素
func processFile(f *descriptorpb.FileDescriptorProto, wg *sync.WaitGroup, context *OptionContext) {
	// 处理文件自身的Option
	if f.Options != nil {
		dispatchOption(OptionTypeFile, f, f.Options, context, wg)
		// dispatchExtensions 也需要同步改造，这里省略，逻辑同dispatchOption
	}

	// 处理Service
	for _, s := range f.Service {
		// Service级上下文：继承File，新增Service
		serviceCtx := &OptionContext{
			File:    context.File,
			Service: s,
		}
		if s.Options != nil {
			dispatchOption(OptionTypeService, s, s.Options, serviceCtx, wg)
		}

		// 处理Method
		for _, m := range s.Method {
			// Method级上下文：继承File和Service
			methodCtx := &OptionContext{
				File:    serviceCtx.File,
				Service: serviceCtx.Service,
			}
			if m.Options != nil {
				dispatchOption(OptionTypeMethod, m, m.Options, methodCtx, wg)
			}
		}
	}

	// 处理Message
	for _, msg := range f.MessageType {
		// Message级上下文：继承File
		msgCtx := &OptionContext{
			File:    context.File,
			Message: msg,
		}
		processMessage(msg, wg, msgCtx)
	}
}

// processMessage 处理消息级Option，并递归处理内部元素
func processMessage(msg *descriptorpb.DescriptorProto, wg *sync.WaitGroup, context *OptionContext) {
	// 处理Message自身的Option
	if msg.Options != nil {
		dispatchOption(OptionTypeMessage, msg, msg.Options, context, wg)
	}

	// 处理Field
	for _, field := range msg.Field {
		// Field级上下文：继承File和Message
		fieldCtx := &OptionContext{
			File:    context.File,
			Message: context.Message,
		}
		if field.Options != nil {
			dispatchOption(OptionTypeField, field, field.Options, fieldCtx, wg)
		}
	}

	// 处理嵌套Message
	for _, nested := range msg.NestedType {
		// 嵌套Message上下文：继承File，更新Message为当前嵌套消息
		nestedCtx := &OptionContext{
			File:    context.File,
			Message: nested,
		}
		processMessage(nested, wg, nestedCtx)
	}
}

// dispatchOption 分发Option回调，新增context参数
func dispatchOption(t OptionType, desc interface{}, opts interface{}, context *OptionContext, wg *sync.WaitGroup) {
	callbacks := getCallbacks(t)

	// 空回调快速返回
	if len(callbacks) == 0 {
		logger.Global.Debug("无可用的Option回调",
			zap.Int("option_type", int(t)),
			zap.String("file", context.File.GetName()), // 日志中增加文件信息
		)
		return
	}

	logger.Global.Debug("开始分发Option回调",
		zap.Int("option_type", int(t)),
		zap.Int("callback_count", len(callbacks)),
		zap.String("file", context.File.GetName()),
	)

	for _, cb := range callbacks {
		wg.Add(1)
		go func(fn OptionCallback) {
			defer wg.Done()

			// 执行回调时传入上下文
			if err := fn(desc, opts, context); err != nil {
				logger.Global.Error("Option回调执行失败",
					zap.Int("option_type", int(t)),
					zap.Error(err),
					zap.String("desc_type", getTypeString(desc)),
					zap.String("opts_type", getTypeString(opts)),
					zap.String("file", context.File.GetName()),
				)
				return
			}

			logger.Global.Debug("Option回调执行成功",
				zap.Int("option_type", int(t)),
				zap.String("desc_type", getTypeString(desc)),
				zap.String("opts_type", getTypeString(opts)),
				zap.String("file", context.File.GetName()),
			)
		}(cb)
	}
}

// getTypeString 获取接口类型的字符串描述
func getTypeString(v interface{}) string {
	if v == nil {
		return "nil"
	}
	return fmt.Sprintf("%T", v)
}

// 示例：注册回调函数的函数（补充完整代码）
func RegisterCallback(t OptionType, cb OptionCallback) {
	mu.Lock()
	defer mu.Unlock()
	callbacksMap[t] = append(callbacksMap[t], cb)
}
