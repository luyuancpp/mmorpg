package queue

import (
	"db_sever/config"
	"fmt"
	"testing"
	"time"
)

func TestPut(t *testing.T) {
	config.Load()
	q := NewMsgQueue(config.DBConfig.RoutineNum, config.DBConfig.ChannelBufferNum)

	go func() {
		for i := uint64(0); i < 10; i++ {
			go func(i uint64) {
				for {
					msg := MsgChannel{}
					msg.Key = i
					q.Put(msg)
				}
			}(i)
		}
	}()

	go func() {
		for i := 0; i < 10; i++ {
			go func(i int) {
				for {
					msg := q.Pop(i)
					fmt.Println(msg.Key)
				}
			}(i)
		}
	}()

	for {
		time.Sleep(10 * time.Second)
	}
}
