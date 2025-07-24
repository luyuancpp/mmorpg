package queue

import (
	"fmt"
	"log"
	"sync"

	"github.com/golang/protobuf/proto"
	"github.com/hashicorp/golang-lru"
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
	keyMap           *lru.Cache // key -> queue index 映射
	QueueList        []MsgChannelList
	RoutineNum       int
	ChannelBufferNum uint64
	expandPolicy     AutoExpandPolicy
	Mu               sync.RWMutex
	closed           bool
}

func NewMsgQueue(routineNum int, channelBufferNum uint64, policy AutoExpandPolicy) *MsgQueue {
	keyMap, _ := lru.New(100_000)
	q := &MsgQueue{
		RoutineNum:       routineNum,
		ChannelBufferNum: channelBufferNum,
		QueueList:        make([]MsgChannelList, routineNum),
		keyMap:           keyMap,
		expandPolicy:     policy,
	}

	for i := 0; i < routineNum; i++ {
		q.QueueList[i] = MsgChannelList{
			Data: make(chan MessageTask, channelBufferNum),
		}
	}

	return q
}

func (q *MsgQueue) Put(msg MessageTask) error {
	q.Mu.Lock()
	defer q.Mu.Unlock()

	var index int
	if val, ok := q.keyMap.Get(msg.Key); ok {
		index = val.(int)
	} else {
		index = int(msg.Key % uint64(len(q.QueueList)))
		q.keyMap.Add(msg.Key, index)
	}

	ch := q.QueueList[index].Data

	// 检查是否需要扩容
	if q.expandPolicy.Enabled {
		usage := float64(len(ch)) / float64(cap(ch))
		if usage >= q.expandPolicy.Threshold && uint64(cap(ch)) < q.expandPolicy.MaxSize {
			newCap := uint64(float64(cap(ch)) * q.expandPolicy.Step)
			if newCap > q.expandPolicy.MaxSize {
				newCap = q.expandPolicy.MaxSize
			}

			// 执行同步扩容
			newChan, err := q.expandBufferLocked(index, newCap)
			if err == nil {
				q.QueueList[index].Data = newChan
				ch = newChan // 更新当前使用的 channel
				log.Printf("[MsgQueue] Auto-expanded channel[%d] to size %d", index, newCap)
			} else {
				log.Printf("[MsgQueue] Failed to expand channel[%d]: %v", index, err)
			}
		}
	}

	// ⚠️ 仍在持锁状态，此时没有人能写入或消费，写入安全
	ch <- msg
	return nil
}

func (q *MsgQueue) expandBufferLocked(index int, newSize uint64) (chan MessageTask, error) {
	if index < 0 || index >= len(q.QueueList) {
		return nil, fmt.Errorf("invalid queue index %d", index)
	}

	oldChan := q.QueueList[index].Data
	newChan := make(chan MessageTask, newSize)

	// 顺序地迁移所有已缓冲的消息（不阻塞、不丢、不乱序）
	for {
		select {
		case msg := <-oldChan:
			newChan <- msg
		default:
			// 没有更多缓存消息了，退出
			return newChan, nil
		}
	}
}

func (q *MsgQueue) Pop(index int) (MessageTask, bool) {
	q.Mu.RLock()
	defer q.Mu.RUnlock()

	if q.closed {
		return MessageTask{}, false
	}

	msg, ok := <-q.QueueList[index].Data
	return msg, ok
}

func (q *MsgQueue) Close() {
	q.Mu.Lock()
	defer q.Mu.Unlock()

	if q.closed {
		return
	}
	q.closed = true

	for _, chList := range q.QueueList {
		close(chList.Data)
	}
}

func (q *MsgQueue) TryPop(index int) (MessageTask, bool) {
	q.Mu.RLock()
	defer q.Mu.RUnlock()

	if q.closed {
		return MessageTask{}, false
	}

	select {
	case msg, ok := <-q.QueueList[index].Data:
		return msg, ok
	default:
		return MessageTask{}, false
	}
}

// ExpandBuffer 扩容某个队列的 channel buffer
func (q *MsgQueue) ExpandBuffer(index int, newSize uint64) error {
	q.Mu.Lock()
	defer q.Mu.Unlock()

	if q.closed {
		return fmt.Errorf("cannot expand buffer: queue is closed")
	}
	if index >= len(q.QueueList) {
		return fmt.Errorf("invalid queue index")
	}

	oldChan := q.QueueList[index].Data
	newChan := make(chan MessageTask, newSize)

	// 把旧数据转移到新 channel
loop:
	for {
		select {
		case msg := <-oldChan:
			newChan <- msg
		default:
			break loop
		}
	}

	// 替换 channel
	q.QueueList[index].Data = newChan

	return nil
}

// ExpandWorker 增加新的 worker 及对应队列
// startWorker 函数用于启动 worker 协程
func (q *MsgQueue) ExpandWorker(n int, startWorker func(index int, ch <-chan MessageTask)) error {
	q.Mu.Lock()
	defer q.Mu.Unlock()

	if q.closed {
		return fmt.Errorf("cannot expand: queue is closed")
	}

	oldLen := len(q.QueueList)
	newLen := oldLen + n
	newQueueList := make([]MsgChannelList, newLen)
	copy(newQueueList, q.QueueList)

	for i := oldLen; i < newLen; i++ {
		newQueueList[i] = MsgChannelList{
			Data: make(chan MessageTask, q.ChannelBufferNum),
		}
		go startWorker(i, newQueueList[i].Data)
	}

	q.QueueList = newQueueList
	q.RoutineNum = newLen
	log.Printf("[MsgQueue] Expanded worker from %d to %d", oldLen, newLen)
	return nil
}

func StartWorker(index int, ch <-chan MessageTask) {
	for msg := range ch {
		// 模拟处理
		switch msg.Operation {
		case OpRead:
			log.Printf("worker %d: Read key %d", index, msg.Key)
		case OpWrite:
			log.Printf("worker %d: Write key %d", index, msg.Key)
		}
		msg.Chan <- true
	}
}

func (q *MsgQueue) GetChannel(index int) <-chan MessageTask {
	q.Mu.RLock()
	defer q.Mu.RUnlock()
	return q.QueueList[index].Data
}

func (q *MsgQueue) Len() int {
	q.Mu.RLock()
	defer q.Mu.RUnlock()
	return len(q.QueueList)
}
