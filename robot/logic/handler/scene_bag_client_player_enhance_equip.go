package handler

import (
	"proto/scene"
	"robot/generated/pb/game"
	"robot/logic/gameobject"
)

// EnhanceEquipHandler:强化。被受理就回人物背包 + 装备栏两份全量,外加 success / enhance_level ——
// 掷骰失败也是这个形状(材料已扣,快照必须带回),不走 error_message;只有被拒时响应体才只剩 error_message。
// 与 EquipItemHandler 同一条记录路径(RecordFeatureBody),equip-forge-smoke 的等待方据此认领本次请求的响应。
func SceneBagClientPlayerEnhanceEquipHandler(player *gameobject.Player, response *scene.EnhanceEquipResponse) {
	RecordFeatureBody(player, game.SceneBagClientPlayerEnhanceEquipMessageId, response)
}
