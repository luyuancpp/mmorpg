package prototools

import (
	"log"
	"sync"
)

// OptionType 指定 5 大类型
type OptionType int

const (
	OptionTypeFile OptionType = iota
	OptionTypeService
	OptionTypeMethod
	OptionTypeMessage
	OptionTypeField
)

// OptionCallback 回调函数签名
//
// desc: 对应的 File/Service/Method/Message/Field 描述符
// opts: option 值（用户定义的扩展）
// 返回 error 会被收集
type OptionCallback func(desc interface{}, opts interface{}) error

// registry 存储每种 OptionType 下的多个回调
var registry = map[OptionType][]OptionCallback{}
var regMu sync.RWMutex

// RegisterOptionCallback 注册回调
func RegisterOptionCallback(t OptionType, cb OptionCallback) {
	regMu.Lock()
	defer regMu.Unlock()
	registry[t] = append(registry[t], cb)
}

// dispatchOption 执行所有回调（并发）
func dispatchOption(t OptionType, desc interface{}, opts interface{}, wg *sync.WaitGroup) {

	regMu.RLock()
	cbs := registry[t]
	regMu.RUnlock()

	for _, cb := range cbs {
		wg.Add(1)
		go func(fn OptionCallback) {
			defer wg.Done()

			defer func() {
				if r := recover(); r != nil {
					log.Printf("panic in option callback: %v", r)
				}
			}()

			if err := fn(desc, opts); err != nil {
				log.Fatal(err)
			}

		}(cb)
	}
}
