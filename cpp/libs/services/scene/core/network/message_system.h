#pragma once

#include "google/protobuf/message.h"

#include "entt/src/entt/entt.hpp"

#include "base/core/type_define/type_define.h"

void SendToCentrePlayerById(uint32_t messageId, const google::protobuf::Message& message, Guid player_id);
void SendToCentrePlayerById(uint32_t messageId, const google::protobuf::Message& message, entt::entity player);

