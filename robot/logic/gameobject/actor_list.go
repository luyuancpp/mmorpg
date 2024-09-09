package gameobject

// ActorList 管理游戏中的演员集合
type ActorList struct {
	actors map[uint64]struct{} // 使用 map 来存储演员 ID，结构体用于表示集合中的存在性
}

// NewActorList 创建一个新的 ActorList 实例
func NewActorList() *ActorList {
	return &ActorList{
		actors: make(map[uint64]struct{}),
	}
}

// AddActor 向 ActorList 中添加一个演员 ID
func (al *ActorList) AddActor(id uint64) {
	al.actors[id] = struct{}{}
}

// RemoveActor 从 ActorList 中移除一个演员 ID
func (al *ActorList) RemoveActor(id uint64) {
	delete(al.actors, id)
}

// HasActor 检查 ActorList 中是否包含某个演员 ID
func (al *ActorList) HasActor(id uint64) bool {
	_, exists := al.actors[id]
	return exists
}

// GetAllActors 返回所有演员 ID
func (al *ActorList) GetAllActors() []uint64 {
	var ids []uint64
	for id := range al.actors {
		ids = append(ids, id)
	}
	return ids
}
