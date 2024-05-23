package logic

import (
	"db_server/config"

	"fmt"
	"testing"
)

func TestPut(t *testing.T) {
	config.Load()
	RoutineNum = config.DBConfig.RoutineNum
	ChanelBufferNum = config.DBConfig.ChannelBufferNum
	
	go func() {
		for i := uint64(0); i < 10; i++ {
			msg := MsgChannel{}
			msg.Key = i
			Put(msg)
		}
	}()

	go func() {
		for i := 0; i < 10; i++ {
			msg1 := Pop(i)
			fmt.Println(msg1.Key)
		}
	}()

}
