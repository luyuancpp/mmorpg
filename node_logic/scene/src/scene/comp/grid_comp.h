#pragma once

#include "type_define/type_define.h"

class HexagonsGrid
{
public:
    EntityUnorderedSet entities;
};

using GridId = absl::uint128;
using SceneGridListComp = std::unordered_map<GridId, HexagonsGrid,  absl::Hash<GridId>>;
using GridSet = std::unordered_set<absl::uint128,  absl::Hash<GridId>>;
