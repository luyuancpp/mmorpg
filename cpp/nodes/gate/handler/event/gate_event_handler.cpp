#include "gate_event_handler.h"
#include "thread_context/dispatcher_manager.h"

///<<< BEGIN WRITING YOUR CODE
#include <session/manager/session_manager.h>
#include "muduo/base/Logging.h"
#include "gate_codec.h"
#include "proto/common/base/node.pb.h"
#include "proto/scene/client_player_common.pb.h"
#include "proto/scene/scene.pb.h"
#include "rpc/service_metadata/client_player_common_service_metadata.h"
#include "rpc/service_metadata/scene_service_metadata.h"
#include "table/proto/tip/login_error_tip.pb.h"
#include "thread_context/node_context_manager.h"
#include "network/rpc_session.h"
#include "node/system/node/node_util.h"

// Forward player entry from Gate to Scene via gRPC PlayerEnterGameNode.
// Gate is the bridge: it holds the TCP session and routes the RPC to the
// correct Scene node using the entity ID from RoutePlayerEvent.
static void ForwardPlayerToScene(uint64_t sessionId, uint32_t enterGsType,
                                 uint64_t sceneNodeId, uint64_t playerId, uint64_t sceneId)
{
    if (enterGsType == 0)
        return; // LOGIN_NONE, nothing to forward

    auto &sceneRegistry = tlsNodeContextManager.GetRegistry(SceneNodeService);
    entt::entity sceneEntity{sceneNodeId};
    if (!sceneRegistry.valid(sceneEntity))
    {
        LOG_ERROR << "ForwardPlayerToScene: scene node entity invalid, scene_node_id=" << sceneNodeId;
        return;
    }

    const auto *rpcSession = sceneRegistry.try_get<RpcSession>(sceneEntity);
    if (!rpcSession)
    {
        LOG_ERROR << "ForwardPlayerToScene: RpcSession not found for scene_node_id=" << sceneNodeId;
        return;
    }

    PlayerEnterGameNodeRequest req;
    req.set_player_id(playerId);
    req.set_session_id(sessionId);
    req.set_enter_gs_type(enterGsType);
    req.set_scene_id(sceneId);

    rpcSession->SendRequest(ScenePlayerEnterGameNodeMessageId, req);

    LOG_INFO << "ForwardPlayerToScene: sent PlayerEnterGameNode to scene_node=" << sceneNodeId
             << " player=" << playerId << " session=" << sessionId
             << " scene_id=" << sceneId << " enter_gs_type=" << enterGsType;
}
///<<< END WRITING YOUR CODE
void GateEventHandler::Register()
{
    tlsEcs.dispatcher.sink<contracts::kafka::RoutePlayerEvent>().connect<&GateEventHandler::RoutePlayerEventHandler>();
    tlsEcs.dispatcher.sink<contracts::kafka::KickPlayerEvent>().connect<&GateEventHandler::KickPlayerEventHandler>();
    tlsEcs.dispatcher.sink<contracts::kafka::PlayerDisconnectedEvent>().connect<&GateEventHandler::PlayerDisconnectedEventHandler>();
    tlsEcs.dispatcher.sink<contracts::kafka::PlayerLeaseExpiredEvent>().connect<&GateEventHandler::PlayerLeaseExpiredEventHandler>();
    tlsEcs.dispatcher.sink<contracts::kafka::BindSessionEvent>().connect<&GateEventHandler::BindSessionEventHandler>();
    tlsEcs.dispatcher.sink<contracts::kafka::RedirectToGateEvent>().connect<&GateEventHandler::RedirectToGateEventHandler>();
    tlsEcs.dispatcher.sink<contracts::kafka::PushToPlayerEvent>().connect<&GateEventHandler::PushToPlayerEventHandler>();
    tlsEcs.dispatcher.sink<contracts::kafka::BroadcastToPlayersEvent>().connect<&GateEventHandler::BroadcastToPlayersEventHandler>();
}

void GateEventHandler::UnRegister()
{
    tlsEcs.dispatcher.sink<contracts::kafka::RoutePlayerEvent>().disconnect<&GateEventHandler::RoutePlayerEventHandler>();
    tlsEcs.dispatcher.sink<contracts::kafka::KickPlayerEvent>().disconnect<&GateEventHandler::KickPlayerEventHandler>();
    tlsEcs.dispatcher.sink<contracts::kafka::PlayerDisconnectedEvent>().disconnect<&GateEventHandler::PlayerDisconnectedEventHandler>();
    tlsEcs.dispatcher.sink<contracts::kafka::PlayerLeaseExpiredEvent>().disconnect<&GateEventHandler::PlayerLeaseExpiredEventHandler>();
    tlsEcs.dispatcher.sink<contracts::kafka::BindSessionEvent>().disconnect<&GateEventHandler::BindSessionEventHandler>();
    tlsEcs.dispatcher.sink<contracts::kafka::RedirectToGateEvent>().disconnect<&GateEventHandler::RedirectToGateEventHandler>();
    tlsEcs.dispatcher.sink<contracts::kafka::PushToPlayerEvent>().disconnect<&GateEventHandler::PushToPlayerEventHandler>();
    tlsEcs.dispatcher.sink<contracts::kafka::BroadcastToPlayersEvent>().disconnect<&GateEventHandler::BroadcastToPlayersEventHandler>();
}
void GateEventHandler::RoutePlayerEventHandler(const contracts::kafka::RoutePlayerEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
    const auto sessionId = event.session_id();
    const auto targetNodeId = event.target_node_id();

    auto &sessions = tlsSessionManager.sessions();
    auto it = sessions.find(sessionId);
    if (it == sessions.end())
    {
        LOG_ERROR << "RoutePlayer: session not found, session_id=" << sessionId;
        return;
    }

    it->second.SetNodeId(SceneNodeService, targetNodeId);
    it->second.sceneId = event.scene_id();

    // Use player_id from the event if session doesn't have it yet (BindSession may not have arrived).
    if (event.player_id() != 0 && it->second.playerId == kInvalidGuid)
    {
        it->second.playerId = event.player_id();
    }

    LOG_INFO << "RoutePlayer: assigned scene node, session_id=" << sessionId
             << " scene_node_id=" << targetNodeId
             << " scene_id=" << event.scene_id();

    // Consume pending login type if BindSession arrived before RoutePlayer.
    const auto pendingType = it->second.pendingEnterGsType;
    if (pendingType != 0)
    {
        it->second.pendingEnterGsType = 0;
        ForwardPlayerToScene(sessionId, pendingType, targetNodeId,
                             it->second.playerId, it->second.sceneId);
    }
///<<< END WRITING YOUR CODE
}
void GateEventHandler::KickPlayerEventHandler(const contracts::kafka::KickPlayerEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
    const auto sessionId = event.session_id();
    auto &sessions = tlsSessionManager.sessions();
    auto it = sessions.find(sessionId);
    if (it == sessions.end())
    {
        // Expected during races between disconnect and kick command delivery.
        LOG_DEBUG << "KickPlayer: session not found, already disconnected. session_id=" << sessionId;
        return;
    }

    auto conn = it->second.conn;
    if (!conn || !conn->connected())
    {
        LOG_DEBUG << "KickPlayer: connection already closed. session_id=" << sessionId;
        return;
    }

    // Send kick notification to client before closing.
    GameKickPlayerRequest kickMsg;
    kickMsg.mutable_reason()->set_id(kLoginBeKickByAnOtherAccount);
    MessageContent mc;
    mc.set_message_id(SceneClientPlayerCommonKickPlayerMessageId);
    mc.set_serialized_message(kickMsg.SerializeAsString());
    GetGateCodec().send(conn, mc);

    conn->shutdown();
    LOG_INFO << "KickPlayer: kicked session_id=" << sessionId
             << " player_id=" << it->second.playerId;
///<<< END WRITING YOUR CODE
}
void GateEventHandler::PlayerDisconnectedEventHandler(const contracts::kafka::PlayerDisconnectedEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
void GateEventHandler::PlayerLeaseExpiredEventHandler(const contracts::kafka::PlayerLeaseExpiredEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
void GateEventHandler::BindSessionEventHandler(const contracts::kafka::BindSessionEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
    const auto sessionId = event.session_id();
    const auto playerId = event.player_id();
    const auto enterGsType = event.enter_gs_type();

    auto &sessions = tlsSessionManager.sessions();
    auto it = sessions.find(sessionId);
    if (it == sessions.end())
    {
        LOG_ERROR << "BindSession: session not found, session_id=" << sessionId
                  << " player_id=" << playerId;
        return;
    }

    it->second.playerId = playerId;
    it->second.sessionVersion = event.session_version();

    // If scene node already assigned (same-Gate reconnect), forward immediately.
    if (it->second.HasNodeId(SceneNodeService))
    {
        const auto sceneNodeId = it->second.GetNodeId(SceneNodeService);
        ForwardPlayerToScene(sessionId, enterGsType, sceneNodeId,
                             playerId, it->second.sceneId);
    }
    else
    {
        // Scene not yet assigned — store for RoutePlayerEvent to consume.
        it->second.pendingEnterGsType = enterGsType;
    }

    LOG_INFO << "BindSession: bound session_id=" << sessionId
             << " player_id=" << playerId
             << " enter_gs_type=" << enterGsType;
///<<< END WRITING YOUR CODE
}
void GateEventHandler::RedirectToGateEventHandler(const contracts::kafka::RedirectToGateEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
    const auto sessionId = event.session_id();
    auto &sessions = tlsSessionManager.sessions();
    auto it = sessions.find(sessionId);
    if (it == sessions.end())
    {
        LOG_ERROR << "RedirectToGate: session not found, session_id=" << sessionId;
        return;
    }

    auto conn = it->second.conn;
    if (!conn || !conn->connected())
    {
        LOG_DEBUG << "RedirectToGate: connection already closed, session_id=" << sessionId;
        return;
    }

    // Build client-facing redirect message.
    RedirectToGateNotify notify;
    notify.set_target_ip(event.target_gate_ip());
    notify.set_target_port(event.target_gate_port());
    notify.set_token_payload(event.token_payload().data(), event.token_payload().size());
    notify.set_token_signature(event.token_signature().data(), event.token_signature().size());
    notify.set_token_deadline(event.token_deadline());

    MessageContent mc;
    mc.set_message_id(SceneClientPlayerCommonRedirectToGateMessageId);
    mc.set_serialized_message(notify.SerializeAsString());
    GetGateCodec().send(conn, mc);

    LOG_INFO << "RedirectToGate: sent redirect to player " << event.player_id()
             << " session_id=" << sessionId
             << " target=" << event.target_gate_ip() << ":" << event.target_gate_port();
///<<< END WRITING YOUR CODE
}
void GateEventHandler::PushToPlayerEventHandler(const contracts::kafka::PushToPlayerEvent &event)
{
    ///<<< BEGIN WRITING YOUR CODE
    auto it = tlsSessionManager.sessions().find(event.session_id());
    if (it == tlsSessionManager.sessions().end())
    {
        return;
    }
    GetGateCodec().send(it->second.conn, event.message_content());
    ///<<< END WRITING YOUR CODE
}
void GateEventHandler::BroadcastToPlayersEventHandler(const contracts::kafka::BroadcastToPlayersEvent &event)
{
    ///<<< BEGIN WRITING YOUR CODE
    for (const auto sessionId : event.session_list())
    {
        auto it = tlsSessionManager.sessions().find(sessionId);
        if (it == tlsSessionManager.sessions().end())
        {
            continue;
        }
        GetGateCodec().send(it->second.conn, event.message_content());
    }
    ///<<< END WRITING YOUR CODE
}
