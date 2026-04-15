package table

import (
    pb "shared/generated/pb/table"
)

// ---------------------------------------------------------------------------
// Foreign key helpers for MirrorTable
// ---------------------------------------------------------------------------

// GetMirrorSceneIdRow resolves Mirror.scene_id -> BaseScene row.
func GetMirrorSceneIdRow(row *pb.MirrorTable) (*pb.BaseSceneTable, bool) {
    return BaseSceneTableManagerInstance.FindById(row.SceneId)
}

// GetMirrorMainSceneIdRow resolves Mirror.main_scene_id -> World row.
func GetMirrorMainSceneIdRow(row *pb.MirrorTable) (*pb.WorldTable, bool) {
    return WorldTableManagerInstance.FindById(row.MainSceneId)
}

// ---------------------------------------------------------------------------
// Reverse FK (HasMany): find source rows by FK column value
// ---------------------------------------------------------------------------

// FindMirrorRowsBySceneId returns all Mirror rows whose scene_id == key.
func FindMirrorRowsBySceneId(key uint32) []*pb.MirrorTable {
    return MirrorTableManagerInstance.GetBySceneId(key)
}

// FindMirrorRowsByMainSceneId returns all Mirror rows whose main_scene_id == key.
func FindMirrorRowsByMainSceneId(key uint32) []*pb.MirrorTable {
    return MirrorTableManagerInstance.GetByMainSceneId(key)
}
