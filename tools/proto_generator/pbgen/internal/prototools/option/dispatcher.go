package prototools

import (
	"log"
	"sync"
)

func dispatchOption(t OptionType, desc interface{}, opts interface{}, wg *sync.WaitGroup) {
	callbacks := getCallbacks(t)

	for _, cb := range callbacks {
		wg.Add(1)
		go func(fn OptionCallback) {
			defer wg.Done()

			if err := fn(desc, opts); err != nil {
				log.Printf("option callback error: %v", err)
			}

		}(cb)
	}
}
