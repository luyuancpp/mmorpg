package table

import (
    pb "shared/generated/pb/table"
)

// ---------------------------------------------------------------------------
// Foreign key helpers for MirrorTable
// ---------------------------------------------------------------------------

// GetMirrorSceneIdRow resolves Mirror.scene_id → BaseScene row.
func GetMirrorSceneIdRow(row *pb.MirrorTable) (*pb.BaseSceneTable, bool) {
    return BaseSceneTableManagerInstance.FindById(row.SceneId)
}

// GetMirrorMainSceneIdRow resolves Mirror.main_scene_id → World row.
func GetMirrorMainSceneIdRow(row *pb.MirrorTable) (*pb.WorldTable, bool) {
    return WorldTableManagerInstance.FindById(row.MainSceneId)
}
