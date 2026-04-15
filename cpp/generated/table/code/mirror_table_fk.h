#pragma once
#include <vector>
#include "mirror_table.h"

#include "basescene_table.h"

#include "world_table.h"

// ---------------------------------------------------------------------------
// Foreign key helpers for MirrorTable
// ---------------------------------------------------------------------------

/// Resolve Mirror.scene_id -> BaseScene row.
inline const BaseSceneTable* GetMirrorSceneIdRow(const MirrorTable& row) {
    auto [ptr, _] = BaseSceneTableManager::Instance().FindByIdSilent(row.scene_id());
    return ptr;
}

/// Resolve Mirror.main_scene_id -> World row.
inline const WorldTable* GetMirrorMainSceneIdRow(const MirrorTable& row) {
    auto [ptr, _] = WorldTableManager::Instance().FindByIdSilent(row.main_scene_id());
    return ptr;
}
