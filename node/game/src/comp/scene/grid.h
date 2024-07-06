#pragma once

#include <unordered_map>
#include <absl/hash/hash.h>
#include <absl/numeric/int128.h>

#include "type_define/type_define.h"

class HexagonsGrid
{
public:
    EntitySet entity_list_;
};

using GridKey = absl::uint128;
using SceneGridList = std::unordered_map<GridKey, HexagonsGrid,  absl::Hash<GridKey>>;
using GridSet = std::unordered_set<absl::uint128,  absl::Hash<GridKey>>;