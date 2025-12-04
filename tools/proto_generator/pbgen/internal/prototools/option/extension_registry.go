package prototools

import (
	"sync"

	"go.uber.org/zap" // 引入zap结构化日志字段
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
	"pbgen/logger" // 引入全局logger包
)

type ExtensionCallback func(desc interface{}, value interface{}) error

var (
	extMu       sync.RWMutex
	extRegistry = map[protoreflect.ExtensionType][]ExtensionCallback{}
)

// RegisterExtensionCallback 注册扩展字段回调函数
func RegisterExtensionCallback(ext protoreflect.ExtensionType, cb ExtensionCallback) {
	extMu.Lock()
	defer extMu.Unlock()

	// 空回调校验
	if cb == nil {
		logger.Global.Warn("尝试注册空的Extension回调",
			zap.String("extension_full_name", string(ext.TypeDescriptor().FullName())),
		)
		return
	}

	extRegistry[ext] = append(extRegistry[ext], cb)
	logger.Global.Debug("Extension回调注册成功",
		zap.String("extension_full_name", string(ext.TypeDescriptor().FullName())),
		zap.Int("callback_count", len(extRegistry[ext])),
	)
}

// dispatchExtensions 分发扩展字段回调
func dispatchExtensions(desc interface{}, opts interface{}, wg *sync.WaitGroup) {
	// 类型断言转换为proto.Message
	msgOpts, ok := opts.(proto.Message)
	if !ok {
		logger.Global.Debug("opts不是proto.Message类型，跳过扩展回调分发",
			zap.String("opts_type", getTypeString(opts)),
		)
		return
	}

	extMu.RLock()
	defer extMu.RUnlock()

	// 空注册表快速返回
	if len(extRegistry) == 0 {
		logger.Global.Debug("扩展回调注册表为空，跳过分发",
			zap.String("msg_full_name", string(msgOpts.ProtoReflect().Descriptor().FullName())),
		)
		return
	}

	logger.Global.Debug("开始分发扩展回调",
		zap.String("msg_full_name", string(msgOpts.ProtoReflect().Descriptor().FullName())),
		zap.Int("registered_extension_count", len(extRegistry)),
		zap.String("desc_type", getTypeString(desc)),
	)

	dispatchCount := 0
	for ext, callbacks := range extRegistry {
		extFullName := ext.TypeDescriptor().FullName()
		msgFullName := msgOpts.ProtoReflect().Descriptor().FullName()

		// 1. 检查扩展字段所属的消息类型是否匹配
		if ext.TypeDescriptor().ContainingMessage().FullName() != msgFullName {
			logger.Global.Debug("扩展字段所属消息类型不匹配，跳过",
				zap.String("extension_full_name", string(extFullName)),
				zap.String("extension_containing_msg", string(ext.TypeDescriptor().ContainingMessage().FullName())),
				zap.String("target_msg_full_name", string(msgFullName)),
			)
			continue
		}

		// 2. 安全获取扩展字段值
		val := proto.GetExtension(msgOpts, ext)
		if val == nil {
			logger.Global.Debug("扩展字段值为空，跳过回调",
				zap.String("extension_full_name", string(extFullName)),
				zap.String("msg_full_name", string(msgFullName)),
			)
			continue
		}

		// 3. 空回调列表跳过
		if len(callbacks) == 0 {
			logger.Global.Debug("扩展字段无回调函数，跳过",
				zap.String("extension_full_name", string(extFullName)),
			)
			continue
		}

		// 4. 并发调用回调函数
		dispatchCount += len(callbacks)
		for _, cb := range callbacks {
			wg.Add(1)
			go func(fn ExtensionCallback, desc interface{}, value interface{}, extName string) {
				defer wg.Done()

				if err := fn(desc, value); err != nil {
					logger.Global.Error("Extension回调执行失败",
						zap.String("extension_full_name", extName),
						zap.String("desc_type", getTypeString(desc)),
						zap.String("value_type", getTypeString(value)),
						zap.Error(err),
					)
					return
				}

				logger.Global.Debug("Extension回调执行成功",
					zap.String("extension_full_name", extName),
					zap.String("desc_type", getTypeString(desc)),
					zap.String("value_type", getTypeString(value)),
				)
			}(cb, desc, val, string(extFullName))
		}
	}

	logger.Global.Info("扩展回调分发完成",
		zap.String("msg_full_name", string(msgOpts.ProtoReflect().Descriptor().FullName())),
		zap.Int("dispatched_callback_count", dispatchCount),
		zap.Int("total_registered_extension_count", len(extRegistry)),
	)
}
