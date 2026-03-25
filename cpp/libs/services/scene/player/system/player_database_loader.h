#pragma once

#include "entt/src/entt/entity/entity.hpp"
#include "proto/common/database/mysql_database_table.pb.h"

void PlayerDatabaseMessageFieldsUnmarshal(entt::entity player, const player_database& message);
void PlayerDatabaseMessageFieldsMarshal(entt::entity player, player_database& message);
