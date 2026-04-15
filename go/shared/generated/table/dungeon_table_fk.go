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

// ---------------------------------------------------------------------------
// Reverse FK (HasMany): find source rows by FK column value
// ---------------------------------------------------------------------------

// FindDungeonRowsBySceneId returns all Dungeon rows whose scene_id == key.
func FindDungeonRowsBySceneId(key uint32) []*pb.DungeonTable {
    return DungeonTableManagerInstance.GetBySceneId(key)
}
