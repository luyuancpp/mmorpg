package prototools

import (
	"sync"

	"protogen/logger"

	"go.uber.org/zap"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
)

type ExtensionCallback func(desc interface{}, value interface{}) error

var (
	extMu       sync.RWMutex
	extRegistry = map[protoreflect.ExtensionType][]ExtensionCallback{}
)

// RegisterExtensionCallback registers a callback for a proto extension field.
func RegisterExtensionCallback(ext protoreflect.ExtensionType, cb ExtensionCallback) {
	extMu.Lock()
	defer extMu.Unlock()

	if cb == nil {
		logger.Global.Warn("Attempted to register nil extension callback",
			zap.String("extension_full_name", string(ext.TypeDescriptor().FullName())),
		)
		return
	}

	extRegistry[ext] = append(extRegistry[ext], cb)
	logger.Global.Debug("Extension callback registered",
		zap.String("extension_full_name", string(ext.TypeDescriptor().FullName())),
		zap.Int("callback_count", len(extRegistry[ext])),
	)
}

// dispatchExtensions dispatches callbacks for proto extension fields.
func dispatchExtensions(desc interface{}, opts interface{}, wg *sync.WaitGroup) {
	msgOpts, ok := opts.(proto.Message)
	if !ok {
		logger.Global.Debug("opts is not proto.Message, skipping extension dispatch",
			zap.String("opts_type", getTypeString(opts)),
		)
		return
	}

	extMu.RLock()
	defer extMu.RUnlock()

	if len(extRegistry) == 0 {
		logger.Global.Debug("Extension registry is empty, skipping dispatch",
			zap.String("msg_full_name", string(msgOpts.ProtoReflect().Descriptor().FullName())),
		)
		return
	}

	logger.Global.Debug("Dispatching extension callbacks",
		zap.String("msg_full_name", string(msgOpts.ProtoReflect().Descriptor().FullName())),
		zap.Int("registered_extension_count", len(extRegistry)),
		zap.String("desc_type", getTypeString(desc)),
	)

	dispatchCount := 0
	for ext, callbacks := range extRegistry {
		extFullName := ext.TypeDescriptor().FullName()
		msgFullName := msgOpts.ProtoReflect().Descriptor().FullName()

		if ext.TypeDescriptor().ContainingMessage().FullName() != msgFullName {
			logger.Global.Debug("Extension containing message type mismatch, skipping",
				zap.String("extension_full_name", string(extFullName)),
				zap.String("extension_containing_msg", string(ext.TypeDescriptor().ContainingMessage().FullName())),
				zap.String("target_msg_full_name", string(msgFullName)),
			)
			continue
		}

		val := proto.GetExtension(msgOpts, ext)
		if val == nil {
			logger.Global.Debug("Extension field value is nil, skipping callbacks",
				zap.String("extension_full_name", string(extFullName)),
				zap.String("msg_full_name", string(msgFullName)),
			)
			continue
		}

		if len(callbacks) == 0 {
			logger.Global.Debug("Extension field has no callbacks, skipping",
				zap.String("extension_full_name", string(extFullName)),
			)
			continue
		}

		dispatchCount += len(callbacks)
		for _, cb := range callbacks {
			wg.Add(1)
			go func(fn ExtensionCallback, desc interface{}, value interface{}, extName string) {
				defer wg.Done()

				if err := fn(desc, value); err != nil {
					logger.Global.Error("Extension callback execution failed",
						zap.String("extension_full_name", extName),
						zap.String("desc_type", getTypeString(desc)),
						zap.String("value_type", getTypeString(value)),
						zap.Error(err),
					)
					return
				}

				logger.Global.Debug("Extension callback executed successfully",
					zap.String("extension_full_name", extName),
					zap.String("desc_type", getTypeString(desc)),
					zap.String("value_type", getTypeString(value)),
				)
			}(cb, desc, val, string(extFullName))
		}
	}

	logger.Global.Info("Extension callback dispatch completed",
		zap.String("msg_full_name", string(msgOpts.ProtoReflect().Descriptor().FullName())),
		zap.Int("dispatched_callback_count", dispatchCount),
		zap.Int("total_registered_extension_count", len(extRegistry)),
	)
}
