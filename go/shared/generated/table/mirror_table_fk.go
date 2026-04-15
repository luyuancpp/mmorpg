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

// GetMirrorSceneIdRowById resolves Mirror.scene_id -> BaseScene row (by Mirror id).
func GetMirrorSceneIdRowById(tableId uint32) (*pb.BaseSceneTable, bool) {
    row, ok := MirrorTableManagerInstance.FindById(tableId)
    if !ok {
        return nil, false
    }
    return GetMirrorSceneIdRow(row)
}

// GetMirrorMainSceneIdRow resolves Mirror.main_scene_id -> World row.
func GetMirrorMainSceneIdRow(row *pb.MirrorTable) (*pb.WorldTable, bool) {
    return WorldTableManagerInstance.FindById(row.MainSceneId)
}

// GetMirrorMainSceneIdRowById resolves Mirror.main_scene_id -> World row (by Mirror id).
func GetMirrorMainSceneIdRowById(tableId uint32) (*pb.WorldTable, bool) {
    row, ok := MirrorTableManagerInstance.FindById(tableId)
    if !ok {
        return nil, false
    }
    return GetMirrorMainSceneIdRow(row)
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
