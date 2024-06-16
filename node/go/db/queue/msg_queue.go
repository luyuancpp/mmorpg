package queue

import "github.com/golang/protobuf/proto"

type MsgChannel struct {
	Key       uint64
	Body      proto.Message
	WhereCase string
	Chan      chan bool
}

type MsgChannelList struct {
	Data chan MsgChannel
}

type MsgQueue struct {
	QueueList       []MsgChannelList
	RoutineNum      int
	ChanelBufferNum uint64
}

func NewMsgQueue(RoutineNum int, ChanelBufferNum uint64) *MsgQueue {
	q := new(MsgQueue)
	q.RoutineNum = RoutineNum
	q.ChanelBufferNum = ChanelBufferNum
	q.QueueList = make([]MsgChannelList, q.RoutineNum)
	for i := 0; i < q.RoutineNum; i++ {
		q.QueueList[i] = MsgChannelList{make(chan MsgChannel, ChanelBufferNum)}
	}
	return q
}

func (q *MsgQueue) Put(msg MsgChannel) {
	index := msg.Key % q.ChanelBufferNum
	q.QueueList[index].Data <- msg
}

func (q *MsgQueue) Pop(index int) MsgChannel {
	msg := <-q.QueueList[index].Data
	return msg
}
