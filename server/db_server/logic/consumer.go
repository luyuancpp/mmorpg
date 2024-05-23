package logic

type MsgChannel struct {
	Key   uint64
	Value []byte
}

type MsgChannelList struct {
	Data chan MsgChannel
}

var ConsumerQueue []MsgChannelList
var RoutineNum int
var ChanelBufferNum uint64

func InitConsumerQueue(queueLength int) {
	ConsumerQueue = make([]MsgChannelList, RoutineNum)
	for i := 0; i < RoutineNum; i++ {
		ConsumerQueue[i] = MsgChannelList{make(chan MsgChannel, ChanelBufferNum)}
	}
}

func Put(msg MsgChannel) {
	index := msg.Key % ChanelBufferNum
	ConsumerQueue[index].Data <- msg
}

func Pop(index int) MsgChannel {
	msg := <-ConsumerQueue[index].Data
	return msg
}
