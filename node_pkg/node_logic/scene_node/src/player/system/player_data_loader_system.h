#pragma once
#include "entt/src/entt/entity/registry.hpp"
class  player_database;
void PlayerDatabaseMessageFieldsUnmarshal(entt::entity player, const player_database& message);
void PlayerDatabaseMessageFieldsMarshal(entt::entity player, player_database& message);
class  player_database_1;
void PlayerDatabase1MessageFieldsUnmarshal(entt::entity player, const player_database_1& message);
void PlayerDatabase1MessageFieldsMarshal(entt::entity player, player_database_1& message);
