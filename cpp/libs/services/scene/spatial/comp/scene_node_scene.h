#pragma once

#include <unordered_set>
#include <absl/hash/hash.h>
#include <absl/numeric/int128.h>

#include "type_alias/actor.h"

struct FollowerListComp
{
    EntityUnorderedSet followerList;
};

struct FollowingListComp
{
    EntityUnorderedSet followingList;
};

struct AoiListComp
{
    EntityUnorderedSet aoiList;
};

using InterestListComp = std::unordered_set<absl::uint128, absl::Hash<absl::uint128>>;