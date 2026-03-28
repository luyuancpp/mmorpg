
package table

import (
    pb "game/generated/pb/table"
)

// ============================================================
// Per-column component structs for SceneTable
// ============================================================
// Scalar columns → value components
// Repeated columns → slice components
// ============================================================


type SceneIdComp struct {
    Value uint32
}

type SceneDungeon_idComp struct {
    Value uint32
}


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

func MakeSceneIdComp(row *pb.SceneTable) SceneIdComp {
    return SceneIdComp{Value: row.Id}
}

func MakeSceneDungeon_idComp(row *pb.SceneTable) SceneDungeon_idComp {
    return SceneDungeon_idComp{Value: row.DungeonId}
}

