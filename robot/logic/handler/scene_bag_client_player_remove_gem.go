package handler

import (
	"proto/scene"
	"robot/generated/pb/game"
	"robot/logic/gameobject"
)

// RemoveGemHandler:取下宝石。成功回人物背包(退回一颗宝石)+ 装备栏两份全量;被拒时响应体只有 error_message。
// 与 EquipItemHandler 同一条记录路径(RecordFeatureBody)。
func SceneBagClientPlayerRemoveGemHandler(player *gameobject.Player, response *scene.RemoveGemResponse) {
	RecordFeatureBody(player, game.SceneBagClientPlayerRemoveGemMessageId, response)
}
