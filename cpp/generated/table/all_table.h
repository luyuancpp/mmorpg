#pragma once

#include <functional>

using LoadSuccessCallback = std::function< void() >;

void LoadTables();

void LoadTablesAsync();

void OnTablesLoadSuccess(const LoadSuccessCallback& callback);