package logic

type MsgChannel struct {
	Key   uint64
	Value []byte
}

type MsgChannelList struct {
	Data chan MsgChannel
}

type MsgQueue struct {
	QueueList       []MsgChannelList
	RoutineNum      int
	ChanelBufferNum uint64
}

func NewMsgQueue(RoutineNum int, ) {

	q.QueueList = make([]MsgChannelList, q.RoutineNum)
	for i := 0; i < q.RoutineNum; i++ {
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
