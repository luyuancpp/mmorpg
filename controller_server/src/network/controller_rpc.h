#pragma once

#include "google/protobuf/message.h"

#include "entt/src/entt/entt.hpp"

#include "src/common_type/common_type.h"
#include "src/game_logic/game_registry.h"
#include "src/network/player_session.h"

template <typename RepliedCallback, typename MethodParam, typename StubMethod, typename StubType>
void CallGsPlayerMethod(entt::entity player, RepliedCallback callback, MethodParam rpc,  StubMethod stub_method, StubType stub)
{
    if (!registry.valid(player))
    {
        return;
    }
    auto try_player_session = registry.try_get<PlayerSession>(player);
    if (nullptr == try_player_session)
    {
        return;
    }
    auto gs_it = g_gs_nodes.find(try_player_session->gs_node_id());
    if (gs_it == g_gs_nodes.end())
    {
        return;
    }
    registry.get<StubType>(gs_it->second)->CallMethod<MethodParam, StubMethod>(callback, rpc, stub_method);
}
