#pragma once

#include "proto/common/event/battle_event.pb.h"

class BattleEventHandler
{
public:
    static void BattleSettlementEventHandler(const BattleSettlementEvent& event);
    static void BattleConfirmedEventHandler(const BattleConfirmedEvent& event);
};
