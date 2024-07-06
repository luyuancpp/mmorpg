#pragma once

#include "type_define/type_define.h"

class HexagonsGrid
{
public:
    EntitySet entity_list;
};

struct WatcherList
{
    UInt128Set watcher_list;
};

struct MarkerList
{
    UInt128Set marker_list;
};

using GridKey = absl::uint128;
using SceneGridList = std::unordered_map<GridKey, HexagonsGrid,  absl::Hash<GridKey>>;
using GridSet = std::unordered_set<absl::uint128,  absl::Hash<GridKey>>;
