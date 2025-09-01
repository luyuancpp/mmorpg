#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <absl/hash/hash.h>
#include <absl/numeric/int128.h>

#include "entt/src/entt/entity/registry.hpp"

// 类型别名
using Guid = uint64_t;
using NodeId = uint32_t;
using SessionId = uint64_t;
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

using EntityUnorderedSet = std::unordered_set<entt::entity>;
using EntityUnorderedMap = std::unordered_map<entt::entity, entt::entity>;
using EntityVector = std::vector<entt::entity>;

using U64U64UnorderedMap = std::unordered_map<uint64_t, uint64_t>;
using U32U32UnorderedMap = std::unordered_map<uint32_t, uint32_t>;
using UInt32UInt64UnorderedMap = std::unordered_map<uint32_t, uint64_t>;
using GuidUint64Map = std::unordered_map<Guid, uint64_t>;

using PlayerPairList = std::unordered_set<absl::uint128, absl::Hash<absl::uint128>>;
using UInt128Set = std::unordered_set<absl::uint128, absl::Hash<absl::uint128>>;

constexpr Guid kInvalidGuid{UINT64_MAX};
constexpr uint64_t kInvalidSessionId{UINT64_MAX};
constexpr uint32_t kInvalidU32Id{UINT32_MAX};
constexpr NodeId kInvalidNodeId{UINT32_MAX};
