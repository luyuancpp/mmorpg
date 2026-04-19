#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <absl/hash/hash.h>
#include <absl/numeric/int128.h>

#include "entt/src/entt/entity/entity.hpp"

// --- Semantic ID types ---
using Guid = uint64_t;
using NodeId = uint32_t;
using SessionId = uint32_t;

// --- Common container aliases (only for frequently-used combos) ---
using GuidVector = std::vector<Guid>;
using StringVector = std::vector<std::string>;
using Int32Vector = std::vector<int32_t>;
using Int64Vector = std::vector<int64_t>;
using UInt32Vector = std::vector<uint32_t>;
using UInt64Vector = std::vector<uint64_t>;

using UInt32Pair = std::pair<uint32_t, uint32_t>;

using UInt32Set = std::unordered_set<uint32_t>;
using UInt64Set = std::unordered_set<uint64_t>;
using UInt32PairSet = std::set<UInt32Pair>;

// --- Entity container aliases ---
using EntityUnorderedSet = std::unordered_set<entt::entity>;
using EntityUnorderedMap = std::unordered_map<entt::entity, entt::entity>;
using EntityVector = std::vector<entt::entity>;

// --- Domain map aliases (use domain-specific names where possible) ---
using ItemCountMap = std::unordered_map<uint32_t, uint32_t>;

// --- 128-bit identity pairs ---
using PlayerPairList = std::unordered_set<absl::uint128, absl::Hash<absl::uint128>>;
using UInt128Set = std::unordered_set<absl::uint128, absl::Hash<absl::uint128>>;

// --- Sentinel values ---
constexpr Guid kInvalidGuid{UINT64_MAX};
constexpr uint32_t kInvalidSessionId{UINT32_MAX};
constexpr uint32_t kInvalidU32Id{UINT32_MAX};
constexpr NodeId kInvalidNodeId{UINT32_MAX};
