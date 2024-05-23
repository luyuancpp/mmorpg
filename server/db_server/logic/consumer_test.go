package logic

import (
	"db_sever/config"
	"fmt"
	"testing"
	"time"
)

func TestPut(t *testing.T) {
	config.Load()
	RoutineNum = config.DBConfig.RoutineNum
	ChanelBufferNum = config.DBConfig.ChannelBufferNum

	InitConsumerQueue()

	go func() {
		for i := uint64(0); i < 10; i++ {
			go func(i uint64) {
				for {
					msg := MsgChannel{}
					msg.Key = i
					Put(msg)
				}
			}(i)
		}
	}()

	go func() {
		for i := 0; i < 10; i++ {
			go func(i int) {
				for {
					msg := Pop(i)
					fmt.Println(msg.Key)
				}
			}(i)
		}
	}()

	for {
		time.Sleep(10 * time.Second)
	}
}
