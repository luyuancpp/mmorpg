#pragma once

#include "src/comp/account_player.h"

using PlayerPtr = std::shared_ptr<AccountPlayer>;
using LoginConnectionMap = std::unordered_map<Guid, PlayerPtr>;