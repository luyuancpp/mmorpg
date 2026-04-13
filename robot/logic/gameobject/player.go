package gameobject

import "sync"

// Player represents a robot's in-game player state.
type Player struct {
	ID uint64
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
