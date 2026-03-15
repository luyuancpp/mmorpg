#pragma once

#include "proto/contracts/kafka/gate_event.pb.h"

class GateEventHandler
{
public:
    static void Register();

    static void UnRegister();
    static void RoutePlayerEventHandler(const contracts::kafka::RoutePlayerEvent& event);
    static void KickPlayerEventHandler(const contracts::kafka::KickPlayerEvent& event);
    static void BindSessionEventHandler(const contracts::kafka::BindSessionEvent& event);
    static void PlayerLeaseExpiredEventHandler(const contracts::kafka::PlayerLeaseExpiredEvent& event);
};
