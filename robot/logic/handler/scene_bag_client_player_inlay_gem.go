package handler

import (
	"proto/scene"
	"robot/generated/pb/game"
	"robot/logic/gameobject"
)

// InlayGemHandler:镶嵌。成功回人物背包(少一颗宝石)+ 装备栏两份全量;被拒时响应体只有 error_message。
// 与 EquipItemHandler 同一条记录路径(RecordFeatureBody)。
func SceneBagClientPlayerInlayGemHandler(player *gameobject.Player, response *scene.InlayGemResponse) {
	RecordFeatureBody(player, game.SceneBagClientPlayerInlayGemMessageId, response)
}
