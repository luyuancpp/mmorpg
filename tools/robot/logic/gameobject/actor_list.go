package gameobject

import (
	"crypto/rand"
	"math/big"
	"sync"
)

// ActorList 管理游戏中的演员集合
type ActorList struct {
	mu     sync.RWMutex
	actors map[uint64]struct{} // 使用 map 来存储演员 ID
}

// NewActorList 创建一个新的 ActorList 实例
func NewActorList() *ActorList {
	return &ActorList{
		actors: make(map[uint64]struct{}),
	}
}

// AddActor 向 ActorList 中添加一个演员 ID
func (al *ActorList) AddActor(id uint64) {
	al.mu.Lock()
	defer al.mu.Unlock()
	al.actors[id] = struct{}{}
}

// RemoveActor 从 ActorList 中移除一个演员 ID
func (al *ActorList) RemoveActor(id uint64) {
	al.mu.Lock()
	defer al.mu.Unlock()
	delete(al.actors, id)
}

// HasActor 检查 ActorList 中是否包含某个演员 ID
func (al *ActorList) HasActor(id uint64) bool {
	al.mu.RLock()
	defer al.mu.RUnlock()
	_, exists := al.actors[id]
	return exists
}

// GetAllActors 返回所有演员 ID
func (al *ActorList) GetAllActors() []uint64 {
	al.mu.RLock()
	defer al.mu.RUnlock()
	var ids []uint64
	for id := range al.actors {
		ids = append(ids, id)
	}
	return ids
}

// GetRandomActor 返回随机一个演员 ID
func (al *ActorList) GetRandomActor() (uint64, bool) {
	al.mu.RLock()
	defer al.mu.RUnlock()
	if len(al.actors) == 0 {
		return 0, false
	}

	// 随机选择一个演员 ID
	var actorIDs []uint64
	for id := range al.actors {
		actorIDs = append(actorIDs, id)
	}

	actorLen := int64(len(actorIDs))
	n, err := rand.Int(rand.Reader, big.NewInt(actorLen))
	if err != nil {
		panic(err)
	}

	return actorIDs[n.Int64()], true
}

// GetActor 根据演员 ID 返回演员
func (al *ActorList) GetActor(id uint64) (struct{}, bool) {
	al.mu.RLock()
	defer al.mu.RUnlock()
	_, exists := al.actors[id]
	return struct{}{}, exists
}
