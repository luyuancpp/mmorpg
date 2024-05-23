package logic

type MsgChannel struct {
	Key   uint64
	Value []byte
}

type MsgChannelList struct {
	Data chan MsgChannel
}

var QueueList []MsgChannelList
var RoutineNum int
var ChanelBufferNum uint64

func InitConsumerQueue() {
	QueueList = make([]MsgChannelList, RoutineNum)
	for i := 0; i < RoutineNum; i++ {
		QueueList[i] = MsgChannelList{make(chan MsgChannel, ChanelBufferNum)}
	}
}

func Put(msg MsgChannel) {
	index := msg.Key % ChanelBufferNum
	QueueList[index].Data <- msg
}

func Pop(index int) MsgChannel {
	msg := <-QueueList[index].Data
	return msg
}
