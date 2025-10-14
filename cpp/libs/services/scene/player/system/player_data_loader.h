#pragma once
#include "entt/src/entt/entity/registry.hpp"
#include "proto/logic/database/mysql_database_table.pb.h"
void PlayerDatabaseMessageFieldsUnmarshal(entt::entity player, const player_database& message);
void PlayerDatabaseMessageFieldsMarshal(entt::entity player, player_database& message);

void PlayerDatabase1MessageFieldsUnmarshal(entt::entity player, const player_database_1& message);
void PlayerDatabase1MessageFieldsMarshal(entt::entity player, player_database_1& message);


void PlayerAllDataMessageFieldsMarshal(entt::entity player, PlayerAllData& message)
{
PlayerDatabaseMessageFieldsMarshal(player, *message.mutable_player_database_data());
PlayerDatabase1MessageFieldsMarshal(player, *message.mutable_player_database_1_data());
}

void PlayerAllDataMessageFieldsUnMarshal(entt::entity player, const PlayerAllData& message)
{
PlayerDatabaseMessageFieldsUnmarshal(player, message.player_database_data());
PlayerDatabase1MessageFieldsUnmarshal(player, message.player_database_1_data());
}
