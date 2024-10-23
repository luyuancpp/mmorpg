#pragma once

#include "type_define/type_define.h"

class HexagonsGrid
{
public:
    EntityUnorderedSet entity_list;
};

using GridKey = absl::uint128;
using SceneGridListComp = std::unordered_map<GridKey, HexagonsGrid,  absl::Hash<GridKey>>;
using GridSet = std::unordered_set<absl::uint128,  absl::Hash<GridKey>>;
