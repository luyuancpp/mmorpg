package handler

import (
	"proto/scene"
	"robot/logic/gameobject"
)

// GetSyncDigest(253)的应答类型是 Empty,scene 不回包:落地摘要走 NotifySyncDigest(254)推送
// (docs/design/landing-recovery-sync-digest.md §4.1)。能走到这里的只有 gate 对 253 的信封拒绝(限流 / 不认识的消息号):
// 拒绝码在 MessageContent.error_message 上,本函数拿到的只是空消息体。
//
// 所以这里不做任何事、不打日志。sync-smoke 在自己的 RecvLoop 回调里先按到达顺序留底、再转发到通用分发,
// 信封拒绝由它从留底里识别(sync_smoke_scenario.go waitDigest)。
func SceneSceneClientPlayerGetSyncDigestHandler(player *gameobject.Player, response *scene.GetSyncDigestRequest) {
}
