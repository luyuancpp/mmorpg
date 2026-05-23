#pragma once
#include "entt/src/entt/entity/registry.hpp"
#include "proto/common/database/player_cache.pb.h"
#include "services/scene/player/system/bag_marshal.h"
void PlayerDatabaseMessageFieldsUnmarshal(entt::entity player, const player_database& message);
void PlayerDatabaseMessageFieldsMarshal(entt::entity player, player_database& message);

void PlayerDatabase1MessageFieldsUnmarshal(entt::entity player, const player_database_1& message);
void PlayerDatabase1MessageFieldsMarshal(entt::entity player, player_database_1& message);

inline void PlayerAllDataMessageFieldsMarshal(entt::entity player, PlayerAllData& message)
{
PlayerDatabaseMessageFieldsMarshal(player, *message.mutable_player_database_data());
PlayerDatabase1MessageFieldsMarshal(player, *message.mutable_player_database_1_data());
// Bag — currently a stub (Bag class not yet attached to player entity in
// production; see bag_marshal.h header notes + task #21). The proto wire
// path is plumbed so the field travels with PlayerAllData; once bag-to-
// entity attach lands, only bag_marshal::Marshal needs filling, no
// caller change.
bag_marshal::Marshal(player, *message.mutable_bag_data());
// Quest / Mail intentionally unwired:
// • Quest module doesn't exist in cpp yet (task #22 audit pending).
// • Mail planned to ride on an independent Go service per
//   cross-zone-readiness-audit.md §3.3, NOT through PlayerAllData.
// Field numbers 5/6 in PlayerAllData stay reserved for these to keep
// future schema migrations stable.
}

inline void PlayerAllDataMessageFieldsUnMarshal(entt::entity player, const PlayerAllData& message)
{
PlayerDatabaseMessageFieldsUnmarshal(player, message.player_database_data());
PlayerDatabase1MessageFieldsUnmarshal(player, message.player_database_1_data());
bag_marshal::Unmarshal(player, message.bag_data());
}
