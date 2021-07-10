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
    using I16V = std::vector<int16_t>;
    using UI16V = std::vector<uint16_t>;
    using I32V = std::vector<int32_t>;
    using I64V = std::vector<int64_t>;
    using UI32V = std::vector<uint32_t>;
    using UI64V = std::vector<uint64_t>;

    using UI64UI64UMap = std::unordered_map<uint64_t, uint64_t>;
    using GameGuidU64Map = std::unordered_map<GameGuid, uint64_t>;

    static const GameGuid kEmptyGameGuid{ 0 };
}//namespace common

#endif//COMMON_SRC_COMMON_TYPE_CommonType
