package gameobject

import (
	"context"
	"math/rand"
	"sync"
)

// Player represents a robot's in-game player state.
type Player struct {
	ID uint64

	mu             sync.RWMutex
	entityID       uint64   // own entt entity in scene
	knownEntities  []uint64 // all visible entities (including self)
	ownedSkillIDs  []uint32 // skills returned by GetSkillList
	sceneID        uint64
	sceneConfigID  uint32
	sceneEnterCnt  int
	skillAckCnt    int
	skillUsedCnt   int
	lastSkillError string

	sceneReady     chan struct{} // closed when first NotifyEnterScene arrives
	sceneReadyOnce sync.Once
}

// NewPlayer creates a Player with an initialized scene-ready channel.
func NewPlayer(id uint64) *Player {
	return &Player{
		ID:         id,
		sceneReady: make(chan struct{}),
	}
}

// SetEntityID sets the player's own entity ID.
func (p *Player) SetEntityID(id uint64) {
	p.mu.Lock()
	defer p.mu.Unlock()
	p.entityID = id
	for _, e := range p.knownEntities {
		if e == id {
			return
		}
	}
	p.knownEntities = append(p.knownEntities, id)
}

// GetEntityID returns the player's own entity ID.
func (p *Player) GetEntityID() uint64 {
	p.mu.RLock()
	defer p.mu.RUnlock()
	return p.entityID
}

// SetSceneInfo updates the player's current scene snapshot.
func (p *Player) SetSceneInfo(sceneID uint64, sceneConfigID uint32) {
	p.mu.Lock()
	defer p.mu.Unlock()
	if p.sceneID != 0 && p.sceneID != sceneID {
		p.entityID = 0
		p.knownEntities = nil
	}
	p.sceneID = sceneID
	p.sceneConfigID = sceneConfigID
	p.sceneEnterCnt++
}

// SignalSceneReady marks the player as having entered a scene.
func (p *Player) SignalSceneReady() {
	p.sceneReadyOnce.Do(func() { close(p.sceneReady) })
}

// WaitSceneReady blocks until the scene is ready or the context is cancelled.
func (p *Player) WaitSceneReady(ctx context.Context) error {
	select {
	case <-p.sceneReady:
		return nil
	case <-ctx.Done():
		return ctx.Err()
	}
}

func (p *Player) GetSceneID() uint64 {
	p.mu.RLock()
	defer p.mu.RUnlock()
	return p.sceneID
}

func (p *Player) GetSceneConfigID() uint32 {
	p.mu.RLock()
	defer p.mu.RUnlock()
	return p.sceneConfigID
}

func (p *Player) GetSceneEnterCount() int {
	p.mu.RLock()
	defer p.mu.RUnlock()
	return p.sceneEnterCnt
}

// AddEntity adds an entity to the known list.
func (p *Player) AddEntity(id uint64) {
	p.mu.Lock()
	defer p.mu.Unlock()
	for _, e := range p.knownEntities {
		if e == id {
			return
		}
	}
	p.knownEntities = append(p.knownEntities, id)
}

// RemoveEntity removes an entity from the known list.
func (p *Player) RemoveEntity(id uint64) {
	p.mu.Lock()
	defer p.mu.Unlock()
	for i, e := range p.knownEntities {
		if e == id {
			p.knownEntities = append(p.knownEntities[:i], p.knownEntities[i+1:]...)
			return
		}
	}
}

// RemoveEntities removes multiple entities from the known list.
func (p *Player) RemoveEntities(ids []uint64) {
	p.mu.Lock()
	defer p.mu.Unlock()
	removeSet := make(map[uint64]struct{}, len(ids))
	for _, id := range ids {
		removeSet[id] = struct{}{}
	}
	filtered := p.knownEntities[:0]
	for _, e := range p.knownEntities {
		if _, ok := removeSet[e]; !ok {
			filtered = append(filtered, e)
		}
	}
	p.knownEntities = filtered
}

// GetRandomEntity returns a random known entity ID, or 0 if none.
func (p *Player) GetRandomEntity() uint64 {
	p.mu.RLock()
	defer p.mu.RUnlock()
	if len(p.knownEntities) == 0 {
		return p.entityID
	}
	return p.knownEntities[rand.Intn(len(p.knownEntities))]
}

func (p *Player) NoteSkillResponse(err string) {
	p.mu.Lock()
	defer p.mu.Unlock()
	if err == "" {
		p.skillAckCnt++
		p.lastSkillError = ""
		return
	}
	p.lastSkillError = err
}

func (p *Player) NoteSkillUsed() {
	p.mu.Lock()
	defer p.mu.Unlock()
	p.skillUsedCnt++
}

func (p *Player) GetSkillStats() (ackCount, usedCount int, lastErr string) {
	p.mu.RLock()
	defer p.mu.RUnlock()
	return p.skillAckCnt, p.skillUsedCnt, p.lastSkillError
}

// SetOwnedSkillIDs sets the player's owned skill IDs.
func (p *Player) SetOwnedSkillIDs(ids []uint32) {
	p.mu.Lock()
	defer p.mu.Unlock()
	p.ownedSkillIDs = ids
}

// GetOwnedSkillIDs returns a copy of the player's owned skill IDs.
func (p *Player) GetOwnedSkillIDs() []uint32 {
	p.mu.RLock()
	defer p.mu.RUnlock()
	if len(p.ownedSkillIDs) == 0 {
		return nil
	}
	out := make([]uint32, len(p.ownedSkillIDs))
	copy(out, p.ownedSkillIDs)
	return out
}

// playerMap is a concurrent-safe map of player ID → *Player.
type playerMap struct {
	mu sync.RWMutex
	m  map[uint64]*Player
}

// PlayerList is the global player registry used by message handlers.
var PlayerList = &playerMap{m: make(map[uint64]*Player)}

func (pm *playerMap) Get(id uint64) (*Player, bool) {
	pm.mu.RLock()
	defer pm.mu.RUnlock()
	p, ok := pm.m[id]
	return p, ok
}

func (pm *playerMap) Set(id uint64, p *Player) {
	pm.mu.Lock()
	defer pm.mu.Unlock()
	pm.m[id] = p
}

func (pm *playerMap) Delete(id uint64) {
	pm.mu.Lock()
	defer pm.mu.Unlock()
	delete(pm.m, id)
}
