#ifndef COMMON_SRC_COMMON_TYPE_CommonType
#define COMMON_SRC_COMMON_TYPE_CommonType

#include <cstdint>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "entt/src/entt/entity/registry.hpp"

namespace common
{
    using Guid = uint64_t;

    using GuidVector = std::vector<Guid>;
    using StringVector = std::vector<std::string>;
    using Int16Vector = std::vector<int16_t>;
    using UInt16Vector = std::vector<uint16_t>;
    using Int32Vector = std::vector<int32_t>;
    using Int64Vector = std::vector<int64_t>;
    using UInt32Vector = std::vector<uint32_t>;
    using UInt64Vector = std::vector<uint64_t>;

	using UInt32Pair = std::pair<uint32_t, uint32_t>;

    using UInt32Set = std::unordered_set<uint32_t>;
    using UInt64Set = std::unordered_set<uint64_t>;
    using UInt32PairSet = std::set<UInt32Pair>;

    using EntitySet = std::unordered_set<entt::entity>;

    using UInt64UInt64UnorderedMap = std::unordered_map<uint64_t, uint64_t>;
    using GuidUint64Map = std::unordered_map<Guid, uint64_t>;

    static const Guid kInvalidGuid{ 0 };
    static const uint32_t kInvalidU32Id{ UINT32_MAX };
}//namespace common

#endif//COMMON_SRC_COMMON_TYPE_CommonType
