package queue

import "github.com/golang/protobuf/proto"

type OperationType int

const (
	OpRead OperationType = iota
	OpWrite
)

type MsgChannel struct {
	Key       uint64
	Body      proto.Message
	WhereCase string
	Chan      chan bool
	Operation OperationType
}

type MsgChannelList struct {
	Data chan MsgChannel
}

type MsgQueue struct {
	QueueList       []MsgChannelList
	RoutineNum      int
	ChanelBufferNum uint64
}

func NewMsgQueue(routineNum int, channelBufferNum uint64) *MsgQueue {
	q := &MsgQueue{
		RoutineNum: routineNum,
		QueueList:  make([]MsgChannelList, routineNum),
	}
	for i := 0; i < routineNum; i++ {
		q.QueueList[i] = MsgChannelList{
			Data: make(chan MsgChannel, channelBufferNum),
		}
	}
	return q
}

func (q *MsgQueue) Put(msg MsgChannel) {
	index := msg.Key % uint64(q.RoutineNum)
	q.QueueList[index].Data <- msg
}

func (q *MsgQueue) Pop(index int) MsgChannel {
	msg := <-q.QueueList[index].Data
	return msg
}
