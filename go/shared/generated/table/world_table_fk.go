package table

import (
    pb "shared/generated/pb/table"
)

// ---------------------------------------------------------------------------
// Foreign key helpers for WorldTable
// ---------------------------------------------------------------------------

// GetWorldSceneIdRow resolves World.scene_id → BaseScene row.
func GetWorldSceneIdRow(row *pb.WorldTable) (*pb.BaseSceneTable, bool) {
    return BaseSceneTableManagerInstance.FindById(row.SceneId)
}
