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
    using UiddLowType = uint64_t; //
    using GameGuid = uint64_t;//std::tuple<UiddHighType, UiddLowType>;

    /*struct guid_hash
    {
        std::size_t operator() (const GameGuid& p) const {
            return std::hash<UiddHighType>{}(std::get<0>(p)) ^ std::hash<UiddLowType>{}(std::get<1>(p));
        }
    };

    struct guid_equal
    {
        bool operator() (const GameGuid& first, const GameGuid& second) const {
            return std::get<0>(first) == std::get<0>(second) &&
                std::get<1>(first) == std::get<1>(second);
        }
    };*/

    using PlayerIdsV = std::vector<GameGuid>;
    using StringV = std::vector<std::string>;

    static const GameGuid kEmptyGameGuid{ 0 };
}//namespace common

#endif//COMMON_SRC_COMMON_TYPE_CommonType
