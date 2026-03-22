package prototools

import (
	"fmt"
	"sync"

	"protogen/logger"

	"go.uber.org/zap"
	"google.golang.org/protobuf/types/descriptorpb"
)

// OptionType defines option scope levels.
type OptionType int

const (
	OptionTypeFile OptionType = iota
	OptionTypeService
	OptionTypeMethod
	OptionTypeMessage
	OptionTypeField
)

// OptionCallback is the handler function type for option processing.
type OptionCallback func(desc interface{}, opts interface{}, context *OptionContext) error

// OptionContext carries parent scope info for each option element.
type OptionContext struct {
	File    *descriptorpb.FileDescriptorProto    // 所属文件（所有类型都有）
	Service *descriptorpb.ServiceDescriptorProto // 所属Service（仅Method有）
	Message *descriptorpb.DescriptorProto        // 所属Message（仅Field、嵌套Message有）
}

var (
	callbacksMap = make(map[OptionType][]OptionCallback)
	mu           sync.RWMutex
)

func getCallbacks(t OptionType) []OptionCallback {
	mu.RLock()
	defer mu.RUnlock()
	return callbacksMap[t]
}

// ProcessAllOptions dispatches option callbacks for every file in the descriptor set.
func ProcessAllOptions(wg *sync.WaitGroup, fdSet *descriptorpb.FileDescriptorSet) error {
	for _, f := range fdSet.File {
		context := &OptionContext{File: f}
		processFile(f, wg, context)
	}
	return nil
}

func processFile(f *descriptorpb.FileDescriptorProto, wg *sync.WaitGroup, context *OptionContext) {
	if f.Options != nil {
		dispatchOption(OptionTypeFile, f, f.Options, context, wg)
		// dispatchExtensions 也需要同步改造，这里省略，逻辑同dispatchOption
	}

	for _, s := range f.Service {
		// Service级上下文：继承File，新增Service
		serviceCtx := &OptionContext{
			File:    context.File,
			Service: s,
		}
		if s.Options != nil {
			dispatchOption(OptionTypeService, s, s.Options, serviceCtx, wg)
		}

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

	for _, msg := range f.MessageType {
		// Message级上下文：继承File
		msgCtx := &OptionContext{
			File:    context.File,
			Message: msg,
		}
		processMessage(msg, wg, msgCtx)
	}
}

func processMessage(msg *descriptorpb.DescriptorProto, wg *sync.WaitGroup, context *OptionContext) {
	if msg.Options != nil {
		dispatchOption(OptionTypeMessage, msg, msg.Options, context, wg)
	}

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

	for _, nested := range msg.NestedType {
		nestedCtx := &OptionContext{
			File:    context.File,
			Message: nested,
		}
		processMessage(nested, wg, nestedCtx)
	}
}

func dispatchOption(t OptionType, desc interface{}, opts interface{}, context *OptionContext, wg *sync.WaitGroup) {
	callbacks := getCallbacks(t)

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

func getTypeString(v interface{}) string {
	if v == nil {
		return "nil"
	}
	return fmt.Sprintf("%T", v)
}

// RegisterCallback adds a callback for the given option type.
func RegisterCallback(t OptionType, cb OptionCallback) {
	mu.Lock()
	defer mu.Unlock()
	callbacksMap[t] = append(callbacksMap[t], cb)
}
