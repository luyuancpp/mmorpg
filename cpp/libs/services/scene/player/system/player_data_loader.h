#pragma once
#include "entt/src/entt/entity/registry.hpp"
#include "proto/common/database/mysql_database_table.pb.h"

void PlayerAllDataMessageFieldsMarshal(entt::entity player, PlayerAllData& message)
{
    // TODO: Serialize each player subsystem (inventory, missions, skills, etc.) into message fields
}

void PlayerAllDataMessageFieldsUnMarshal(entt::entity player, const PlayerAllData& message)
{
    // TODO: Deserialize message fields into each player subsystem component
}
