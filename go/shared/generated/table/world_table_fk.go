package table

import (
    pb "shared/generated/pb/table"
)

// ---------------------------------------------------------------------------
// Foreign key helpers for WorldTable
// ---------------------------------------------------------------------------

// GetWorldSceneIdRow resolves World.scene_id -> BaseScene row.
func GetWorldSceneIdRow(row *pb.WorldTable) (*pb.BaseSceneTable, bool) {
    return BaseSceneTableManagerInstance.FindById(row.SceneId)
}

// GetWorldSceneIdRowById resolves World.scene_id -> BaseScene row (by World id).
func GetWorldSceneIdRowById(tableId uint32) (*pb.BaseSceneTable, bool) {
    row, ok := WorldTableManagerInstance.FindById(tableId)
    if !ok {
        return nil, false
    }
    return GetWorldSceneIdRow(row)
}

// ---------------------------------------------------------------------------
// Reverse FK (HasMany): find source rows by FK column value
// ---------------------------------------------------------------------------

// FindWorldRowsBySceneId returns all World rows whose scene_id == key.
func FindWorldRowsBySceneId(key uint32) []*pb.WorldTable {
    return WorldTableManagerInstance.GetBySceneId(key)
}
