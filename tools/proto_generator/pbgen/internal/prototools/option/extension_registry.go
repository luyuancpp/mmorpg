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
	defer extMu.RUnlock()

	for ext, callbacks := range extRegistry {

		val := proto.GetExtension(msgOpts, ext)
		if val == nil {
			continue
		}

		// 触发回调
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
}
