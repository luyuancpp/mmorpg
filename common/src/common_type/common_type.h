#ifndef COMMON_SRC_COMMON_TYPE_CommonType
#define COMMON_SRC_COMMON_TYPE_CommonType

#include <cstdint>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace common
{
    using UiddHighType = uint64_t;
    using UiddLowType = uint64_t; 
    using GameGuid = uint64_t;

    using PlayerIdsV = std::vector<GameGuid>;
    using StringV = std::vector<std::string>;

    static const GameGuid kEmptyGameGuid{ 0 };
}//namespace common

#endif//COMMON_SRC_COMMON_TYPE_CommonType
