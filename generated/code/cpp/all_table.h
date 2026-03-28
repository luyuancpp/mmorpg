
#pragma once

#include <cstdint>
#include <functional>

using LoadSuccessCallback = std::function<void()>;

void LoadTables();

void LoadTablesAsync();

void OnTablesLoadSuccess(const LoadSuccessCallback& callback);

/// Reload all tables and bump the version counter.
/// WARNING: after reload, any string_view/span components built from old
///          proto data are INVALID.  Rebuild all components immediately.
void ReloadTables();

/// Monotonically-increasing version; incremented by ReloadTables().
/// Use to detect stale cached components.
uint64_t GetTableLoadVersion();