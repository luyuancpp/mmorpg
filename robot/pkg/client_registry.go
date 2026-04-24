package pkg

import "sync"

// clientRegistry maps a logged-in player ID to its GameClient so message
// handlers (which only receive the gameobject.Player) can reach the
// underlying connection when they need to mutate client-owned state —
// currently only the access_token pair updated by the RefreshToken handler.
//
// Registration is scoped to the in-game session: register on EnterGame,
// unregister on disconnect / LeaveGame. Handlers MUST tolerate a missing
// entry (Get returns nil) so they don't fight a teardown in progress.
type clientRegistry struct {
	mu sync.RWMutex
	m  map[uint64]*GameClient
}

// Clients is the process-wide GameClient registry.
var Clients = &clientRegistry{m: make(map[uint64]*GameClient)}

// Register records gc under playerID. Overwrites silently (same playerID on
// a new connection during reconnect is expected).
func (r *clientRegistry) Register(playerID uint64, gc *GameClient) {
	if playerID == 0 {
		return
	}
	r.mu.Lock()
	r.m[playerID] = gc
	r.mu.Unlock()
}

// Unregister removes the entry for playerID only if it currently points at
// the given gc. This guards against a racy teardown clobbering a fresh
// reconnect that reused the same playerID.
func (r *clientRegistry) Unregister(playerID uint64, gc *GameClient) {
	if playerID == 0 {
		return
	}
	r.mu.Lock()
	if cur, ok := r.m[playerID]; ok && cur == gc {
		delete(r.m, playerID)
	}
	r.mu.Unlock()
}

// Get returns the GameClient currently registered for playerID, or nil.
func (r *clientRegistry) Get(playerID uint64) *GameClient {
	r.mu.RLock()
	defer r.mu.RUnlock()
	return r.m[playerID]
}
