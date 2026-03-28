package handler

import (
	"robot/logic/gameobject"
	"robot/pkg"
)

// RegisterPlayer creates a Player entry in the global PlayerList
// so that subsequent message handlers can look it up.
func RegisterPlayer(gc *pkg.GameClient) {
	p := &gameobject.Player{
		PlayerId: gc.PlayerId,
		Account:  gc.Account,
	}
	gameobject.PlayerList.Set(gc.PlayerId, p)
}
