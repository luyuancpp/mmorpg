package queue

import (
	"fmt"
	"github.com/golang/protobuf/proto"
	lru "github.com/hashicorp/golang-lru"
	"log"
	"sync"
)

type OperationType int

const (
	OpRead OperationType = iota
	OpWrite
)

type MessageTask struct {
	Key       uint64
	Body      proto.Message
	WhereCase string
	Chan      chan bool
	Operation OperationType
}

type MsgChannelList struct {
	Data chan MessageTask
}

type AutoExpandPolicy struct {
	Threshold float64 // 使用率超过此阈值才触发扩容（如 0.8）
	Step      float64 // 扩容倍数（如 1.5）
	MaxSize   uint64  // 单个 channel 允许的最大容量
	Enabled   bool
}

type MsgQueue struct {
	keyMap           *lru.Cache
	queueList        []MsgChannelList
	RoutineNum       int
	ChannelBufferNum uint64
	expandPolicy     AutoExpandPolicy
	mu               sync.RWMutex
	closed           bool
}

func NewMsgQueue(routineNum int, channelBufferNum uint64, policy AutoExpandPolicy) *MsgQueue {
	keyMap, _ := lru.New(100_000)
	q := &MsgQueue{
		RoutineNum:       routineNum,
		ChannelBufferNum: channelBufferNum,
		queueList:        make([]MsgChannelList, routineNum),
		keyMap:           keyMap,
		expandPolicy:     policy,
	}

	for i := 0; i < routineNum; i++ {
		q.queueList[i] = MsgChannelList{
			Data: make(chan MessageTask, channelBufferNum),
		}
	}

	return q
}

func (q *MsgQueue) Put(msg MessageTask) error {
	q.mu.Lock()
	defer q.mu.Unlock()

	var index int
	if val, ok := q.keyMap.Get(msg.Key); ok {
		index = val.(int)
	} else {
		index = int(msg.Key % uint64(len(q.queueList)))
		q.keyMap.Add(msg.Key, index)
	}

	ch := q.queueList[index].Data

	if q.expandPolicy.Enabled {
		usage := float64(len(ch)) / float64(cap(ch))
		if usage >= q.expandPolicy.Threshold && uint64(cap(ch)) < q.expandPolicy.MaxSize {
			newCap := uint64(float64(cap(ch)) * q.expandPolicy.Step)
			if newCap > q.expandPolicy.MaxSize {
				newCap = q.expandPolicy.MaxSize
			}
			_ = q.expandBufferLocked(index, newCap)
			ch = q.queueList[index].Data
			log.Printf("[MsgQueue] Auto-expanded channel[%d] to size %d", index, newCap)
		}
	}

	ch <- msg
	return nil
}

func (q *MsgQueue) Pop(index int) (MessageTask, bool) {
	q.mu.RLock()
	defer q.mu.RUnlock()

	if q.closed {
		return MessageTask{}, false
	}

	msg, ok := <-q.queueList[index].Data
	return msg, ok
}

func (q *MsgQueue) TryPop(index int) (MessageTask, bool) {
	q.mu.RLock()
	defer q.mu.RUnlock()

	if q.closed {
		return MessageTask{}, false
	}

	select {
	case msg, ok := <-q.queueList[index].Data:
		return msg, ok
	default:
		return MessageTask{}, false
	}
}

func (q *MsgQueue) Close() {
	q.mu.Lock()
	defer q.mu.Unlock()

	if q.closed {
		return
	}
	q.closed = true

	for _, chList := range q.queueList {
		close(chList.Data)
	}
}

func (q *MsgQueue) expandBufferLocked(index int, newSize uint64) error {
	if index < 0 || index >= len(q.queueList) {
		return fmt.Errorf("invalid queue index %d", index)
	}

	oldChan := q.queueList[index].Data
	newChan := make(chan MessageTask, newSize)

	for {
		select {
		case msg, ok := <-oldChan:
			if !ok {
				break
			}
			newChan <- msg
		default:
			goto done
		}
	}
done:
	q.queueList[index].Data = newChan
	return nil
}
