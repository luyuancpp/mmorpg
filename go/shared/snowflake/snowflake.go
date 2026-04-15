package snowflake

import (
	"fmt"
	"sync"
	"time"
)

// Layout matches C++ SnowFlake: [time:32][node:17][step:15]
// Epoch: 2026-03-14 00:00:00 UTC (1773446400)
const (
	Epoch    uint64 = 1773446400
	NodeBits uint64 = 17
	StepBits uint64 = 15

	timeShift = NodeBits + StepBits
	nodeShift = StepBits
	stepMask  = (1 << StepBits) - 1
	NodeMask  = (1 << NodeBits) - 1
)

// Node generates unique 64-bit IDs using the Snowflake algorithm.
// Thread-safe via mutex.
type Node struct {
	mu       sync.Mutex
	nodeID   uint64
	lastTime uint64
	step     uint64
}

// NewNode creates a SnowFlake generator with the given node ID.
// Panics if nodeID exceeds 17-bit range (0..131071).
func NewNode(nodeID uint64) *Node {
	if nodeID > NodeMask {
		panic(fmt.Sprintf("snowflake: node ID %d exceeds max %d", nodeID, NodeMask))
	}
	return &Node{nodeID: nodeID}
}

// Generate produces a globally unique uint64 ID.
func (n *Node) Generate() uint64 {
	n.mu.Lock()
	defer n.mu.Unlock()

	now := nowEpoch()

	if now < n.lastTime {
		// Clock moved backwards — spin until it catches up.
		now = n.waitNextTime(n.lastTime)
	}

	if now > n.lastTime {
		n.lastTime = now
		n.step = 0
	} else {
		if n.step >= stepMask {
			now = n.waitNextTime(n.lastTime)
			n.lastTime = now
			n.step = 0
		} else {
			n.step++
		}
	}

	return (n.lastTime << timeShift) |
		(n.nodeID << nodeShift) |
		n.step
}

func (n *Node) waitNextTime(last uint64) uint64 {
	for {
		now := nowEpoch()
		if now > last {
			return now
		}
		time.Sleep(time.Millisecond)
	}
}

func nowEpoch() uint64 {
	return uint64(time.Now().Unix()) - Epoch
}
