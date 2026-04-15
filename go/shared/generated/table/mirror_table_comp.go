
package table

import (
    pb "shared/generated/pb/table"
)

// ============================================================
// Per-column component structs for MirrorTable
// ============================================================
// Scalar columns → value components
// Repeated columns → slice components
// ============================================================


type MirrorIdComp struct {
    Value uint32
}

type MirrorScene_idComp struct {
    Value uint32
}

type MirrorMain_scene_idComp struct {
    Value uint32
}


// ============================================================
// Factory helpers — build component from a proto row
// ============================================================

func MakeMirrorIdComp(row *pb.MirrorTable) MirrorIdComp {
    return MirrorIdComp{Value: row.Id}
}

func MakeMirrorScene_idComp(row *pb.MirrorTable) MirrorScene_idComp {
    return MirrorScene_idComp{Value: row.SceneId}
}

func MakeMirrorMain_scene_idComp(row *pb.MirrorTable) MirrorMain_scene_idComp {
    return MirrorMain_scene_idComp{Value: row.MainSceneId}
}

