package table

import (
    pb "shared/generated/pb/table"
)

// ---------------------------------------------------------------------------
// Foreign key helpers for DungeonTable
// ---------------------------------------------------------------------------

// GetDungeonSceneIdRow resolves Dungeon.scene_id → BaseScene row.
func GetDungeonSceneIdRow(row *pb.DungeonTable) (*pb.BaseSceneTable, bool) {
    return BaseSceneTableManagerInstance.FindById(row.SceneId)
}
