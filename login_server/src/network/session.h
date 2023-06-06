#pragma once

#include "src/comp/account_player.h"

using PlayerPtr = std::shared_ptr<AccountPlayer>;
using ConnectionEntityMap = std::unordered_map<Guid, PlayerPtr>;