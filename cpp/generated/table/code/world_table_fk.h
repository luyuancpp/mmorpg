#pragma once
#include <vector>
#include "world_table.h"

#include "basescene_table.h"

// ---------------------------------------------------------------------------
// Foreign key helpers for WorldTable
// ---------------------------------------------------------------------------

/// Resolve World.scene_id -> BaseScene row.
inline const BaseSceneTable* GetWorldSceneIdRow(const WorldTable& row) {
    auto [ptr, _] = BaseSceneTableManager::Instance().FindByIdSilent(row.scene_id());
    return ptr;
}

/// Resolve World.scene_id -> BaseScene row (by World id).
inline const BaseSceneTable* GetWorldSceneIdRow(uint32_t tableId) {
    auto [row, _] = WorldTableManager::Instance().FindByIdSilent(tableId);
    if (!row) return nullptr;
    return GetWorldSceneIdRow(*row);
}

// ---------------------------------------------------------------------------
// Reverse FK (HasMany): find source rows by FK column value
// ---------------------------------------------------------------------------

/// Reverse FK: find all World rows whose scene_id == key.
inline const std::vector<const WorldTable*>& FindWorldRowsBySceneId(uint32_t key) {
    return WorldTableManager::Instance().GetBySceneId(key);
}
