package gameobject

import (
	cmap "github.com/orcaman/concurrent-map/v2"
)

// Player represents a connected robot player.
type Player struct {
	PlayerId uint64
	Account  string
}

// PlayerList is a thread-safe map of player ID → *Player.
// Handler goroutines look up players here after login/enter.
var PlayerList = cmap.NewWithCustomShardingFunction[uint64, *Player](func(key uint64) uint32 {
	return uint32(key)
})
