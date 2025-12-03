package prototools

import (
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
	"log"
	"sync"
)

type ExtensionCallback func(desc interface{}, value interface{}) error

var (
	extMu       sync.RWMutex
	extRegistry = map[protoreflect.ExtensionType][]ExtensionCallback{}
)

func RegisterExtensionCallback(ext protoreflect.ExtensionType, cb ExtensionCallback) {
	extMu.Lock()
	defer extMu.Unlock()
	extRegistry[ext] = append(extRegistry[ext], cb)
}

func dispatchExtensions(desc interface{}, opts interface{}, wg *sync.WaitGroup) {
	msgOpts, ok := opts.(proto.Message)
	if !ok {
		return
	}

	extMu.RLock()
	for ext, callbacks := range extRegistry {

		// 1. 检查 opts 类型是否是 extension 的 containing type
		// -----------------------------------------------------------------
		if ext.TypeDescriptor().ContainingMessage().FullName() !=
			msgOpts.ProtoReflect().Descriptor().FullName() {
			continue
		}
		// -----------------------------------------------------------------

		// 2. 安全 GetExtension（不会 panic）
		val := proto.GetExtension(msgOpts, ext)
		if val == nil {
			continue
		}

		// 3. 调用回调
		for _, cb := range callbacks {
			wg.Add(1)
			go func(fn ExtensionCallback, desc interface{}, value interface{}) {
				defer wg.Done()
				if err := fn(desc, value); err != nil {
					log.Printf("extension callback error: %v", err)
				}
			}(cb, desc, val)
		}
	}
	extMu.RUnlock()
}
