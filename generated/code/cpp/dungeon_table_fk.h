#pragma once
#include <vector>
#include "dungeon_table.h"

#include "basescene_table.h"

// ---------------------------------------------------------------------------
// Foreign key helpers for DungeonTable
// ---------------------------------------------------------------------------

/// Resolve Dungeon.scene_id → BaseScene row.
inline const BaseSceneTable* GetSceneIdRow(const DungeonTable& row) {
    auto [ptr, _] = BaseSceneTableManager::Instance().FindByIdSilent(row.scene_id());
    return ptr;
}
