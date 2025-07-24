package queue

import (
	"db/internal/config"
	"fmt"
	"testing"
	"time"
)

func TestPut(t *testing.T) {
	q := NewMsgQueue(config.DBConfig.RoutineNum, config.DBConfig.ChannelBufferNum)

	go func() {
		for i := uint64(0); i < 10; i++ {
			go func(i uint64) {
				for {
					msg := MessageTask{}
					msg.Key = i
					msg.Chan = make(chan bool)
					q.Put(msg)
					_, ok := <-msg.Chan
					if !ok {
						fmt.Println("channel closed")
					} else {
						fmt.Println("put ok")
					}
				}
			}(i)
		}
	}()

	go func() {
		for i := 0; i < 10; i++ {
			go func(i int) {
				for {
					msg := q.Pop(i)
					msg.Chan <- true
				}
			}(i)
		}
	}()

	for {
		time.Sleep(10 * time.Second)
	}
}
