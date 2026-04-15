#pragma once
#include <vector>
#include "world_table.h"

#include "basescene_table.h"

// ---------------------------------------------------------------------------
// Foreign key helpers for WorldTable
// ---------------------------------------------------------------------------

/// Resolve World.scene_id → BaseScene row.
inline const BaseSceneTable* GetSceneIdRow(const WorldTable& row) {
    auto [ptr, _] = BaseSceneTableManager::Instance().FindByIdSilent(row.scene_id());
    return ptr;
}
