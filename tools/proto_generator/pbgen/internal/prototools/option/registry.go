package prototools

import (
	"sync"
)

type OptionType int

const (
	OptionTypeFile OptionType = iota
	OptionTypeService
	OptionTypeMethod
	OptionTypeMessage
	OptionTypeField
)

type OptionCallback func(desc interface{}, opts interface{}) error

var (
	regMu    sync.RWMutex
	registry = map[OptionType][]OptionCallback{}
)

func RegisterOptionCallback(t OptionType, cb OptionCallback) {
	regMu.Lock()
	defer regMu.Unlock()
	registry[t] = append(registry[t], cb)
}

func getCallbacks(t OptionType) []OptionCallback {
	regMu.RLock()
	defer regMu.RUnlock()
	return registry[t]
}
