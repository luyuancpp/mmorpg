#pragma once
#include <vector>
#include "dungeon_table.h"

#include "basescene_table.h"

// ---------------------------------------------------------------------------
// Foreign key helpers for DungeonTable
// ---------------------------------------------------------------------------

/// Resolve Dungeon.scene_id -> BaseScene row.
inline const BaseSceneTable* GetDungeonSceneIdRow(const DungeonTable& row) {
    auto [ptr, _] = BaseSceneTableManager::Instance().FindByIdSilent(row.scene_id());
    return ptr;
}

// ---------------------------------------------------------------------------
// Reverse FK (HasMany): find source rows by FK column value
// ---------------------------------------------------------------------------

/// Reverse FK: find all Dungeon rows whose scene_id == key.
inline std::vector<const DungeonTable*> FindDungeonRowsBySceneId(uint32_t key) {
    return DungeonTableManager::Instance().GetBySceneId(key);
}
