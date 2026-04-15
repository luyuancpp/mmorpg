package table

import (
    pb "shared/generated/pb/table"
)

// ---------------------------------------------------------------------------
// Foreign key helpers for DungeonTable
// ---------------------------------------------------------------------------

// GetDungeonSceneIdRow resolves Dungeon.scene_id -> BaseScene row.
func GetDungeonSceneIdRow(row *pb.DungeonTable) (*pb.BaseSceneTable, bool) {
    return BaseSceneTableManagerInstance.FindById(row.SceneId)
}

// GetDungeonSceneIdRowById resolves Dungeon.scene_id -> BaseScene row (by Dungeon id).
func GetDungeonSceneIdRowById(tableId uint32) (*pb.BaseSceneTable, bool) {
    row, ok := DungeonTableManagerInstance.FindById(tableId)
    if !ok {
        return nil, false
    }
    return GetDungeonSceneIdRow(row)
}

// ---------------------------------------------------------------------------
// Reverse FK (HasMany): find source rows by FK column value
// ---------------------------------------------------------------------------

// FindDungeonRowsBySceneId returns all Dungeon rows whose scene_id == key.
func FindDungeonRowsBySceneId(key uint32) []*pb.DungeonTable {
    return DungeonTableManagerInstance.GetBySceneId(key)
}
