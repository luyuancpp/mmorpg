package handler

import (
	"go.uber.org/zap"

	"proto/scene"
	"robot/logic/gameobject"
)

// NotifySyncDigest(254)是 GetSyncDigest(253)的数据通道:落地恢复的按域版本摘要
// (docs/design/landing-recovery-sync-digest.md §4.1)。
//
// 机器人没有本地摘要缓存,也没有按域拉取的消费者,通用分发只打一行 Debug 便于排障;断言由 sync-smoke 做 ——
// 它在自己的 RecvLoop 回调里先按到达顺序留底、再转发到这里,从请求前记下的留底下标起认领"这次请求带回来的那一份"(sync_smoke_scenario.go)。
// 压测模式的机器人不发 253,正常不会走到这里。日志里的 player_id 只用于排障,不进任何指标 label。
func SceneSceneClientPlayerNotifySyncDigestHandler(player *gameobject.Player, response *scene.SyncDigestS2C) {
	if player == nil || response == nil {
		return
	}
	zap.L().Debug("sync digest received",
		zap.Uint64("player_id", player.ID),
		zap.String("status", response.GetStatus().String()),
		zap.Uint64("generation", response.GetGeneration()),
		zap.Int("entries", len(response.GetEntries())),
	)
}
