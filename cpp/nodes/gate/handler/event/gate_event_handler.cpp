#include "gate_event_handler.h"
#include "thread_context/dispatcher_manager.h"

///<<< BEGIN WRITING YOUR CODE
#include <session/manager/session_manager.h>
#include "muduo/base/Logging.h"
#include "proto/common/base/node.pb.h"
#include "proto/scene/game_player.pb.h"
#include "rpc/service_metadata/game_player_service_metadata.h"
#include "rpc/service_metadata/scene_service_metadata.h"
#include "thread_context/node_context_manager.h"
#include "network/rpc_session.h"
#include "node/system/node/node_util.h"

// Forward login notification from Gate to Scene via gRPC.
static void ForwardLoginToScene(uint64_t sessionId, uint32_t enterGsType, uint64_t sceneNodeId)
{
    if (enterGsType == 0) return; // LOGIN_NONE, nothing to forward

    auto& sceneRegistry = tlsNodeContextManager.GetRegistry(SceneNodeService);
    entt::entity sceneEntity{ sceneNodeId };
    if (!sceneRegistry.valid(sceneEntity)) {
        LOG_ERROR << "ForwardLoginToScene: scene node entity invalid, scene_node_id=" << sceneNodeId;
        return;
    }

    const auto* rpcSession = sceneRegistry.try_get<RpcSession>(sceneEntity);
    if (!rpcSession) {
        LOG_ERROR << "ForwardLoginToScene: RpcSession not found for scene_node_id=" << sceneNodeId;
        return;
    }

    GateLoginNotifyRequest loginReq;
    loginReq.set_enter_gs_type(enterGsType);

    NodeRouteMessageRequest routeReq;
    routeReq.mutable_message_content()->set_message_id(ScenePlayerGateLoginNotifyMessageId);
    routeReq.mutable_message_content()->set_serialized_message(loginReq.SerializeAsString());
    routeReq.mutable_header()->set_session_id(sessionId);

    rpcSession->SendRequest(SceneSendMessageToPlayerMessageId, routeReq);

    LOG_INFO << "ForwardLoginToScene: sent login notification to scene, session_id=" << sessionId
             << " enter_gs_type=" << enterGsType << " scene_node_id=" << sceneNodeId;
}
///<<< END WRITING YOUR CODE
void GateEventHandler::Register()
{
    tlsEcs.dispatcher.sink<contracts::kafka::RoutePlayerEvent>().connect<&GateEventHandler::RoutePlayerEventHandler>();
    tlsEcs.dispatcher.sink<contracts::kafka::KickPlayerEvent>().connect<&GateEventHandler::KickPlayerEventHandler>();
    tlsEcs.dispatcher.sink<contracts::kafka::PlayerDisconnectedEvent>().connect<&GateEventHandler::PlayerDisconnectedEventHandler>();
    tlsEcs.dispatcher.sink<contracts::kafka::PlayerLeaseExpiredEvent>().connect<&GateEventHandler::PlayerLeaseExpiredEventHandler>();
    tlsEcs.dispatcher.sink<contracts::kafka::BindSessionEvent>().connect<&GateEventHandler::BindSessionEventHandler>();
}

void GateEventHandler::UnRegister()
{
    tlsEcs.dispatcher.sink<contracts::kafka::RoutePlayerEvent>().disconnect<&GateEventHandler::RoutePlayerEventHandler>();
    tlsEcs.dispatcher.sink<contracts::kafka::KickPlayerEvent>().disconnect<&GateEventHandler::KickPlayerEventHandler>();
    tlsEcs.dispatcher.sink<contracts::kafka::PlayerDisconnectedEvent>().disconnect<&GateEventHandler::PlayerDisconnectedEventHandler>();
    tlsEcs.dispatcher.sink<contracts::kafka::PlayerLeaseExpiredEvent>().disconnect<&GateEventHandler::PlayerLeaseExpiredEventHandler>();
    tlsEcs.dispatcher.sink<contracts::kafka::BindSessionEvent>().disconnect<&GateEventHandler::BindSessionEventHandler>();
}
void GateEventHandler::RoutePlayerEventHandler(const contracts::kafka::RoutePlayerEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
    auto sessionIt = tlsSessionManager.sessions().find(event.session_id());
    if (sessionIt == tlsSessionManager.sessions().end()) {
        LOG_WARN << "RoutePlayer ignored, session not found. session_id=" << event.session_id()
            << ", target_node_id=" << event.target_node_id();
        return;
    }

    sessionIt->second.SetNodeId(SceneNodeService, event.target_node_id());
    LOG_INFO << "RoutePlayer applied. session_id=" << event.session_id()
        << ", scene_node_id=" << event.target_node_id();

    // If there's a pending login notification (e.g. FirstLogin), forward it now that scene is assigned.
    if (sessionIt->second.pendingEnterGsType != 0) {
        ForwardLoginToScene(event.session_id(), sessionIt->second.pendingEnterGsType, event.target_node_id());
        sessionIt->second.pendingEnterGsType = 0;
    }
///<<< END WRITING YOUR CODE
}
void GateEventHandler::KickPlayerEventHandler(const contracts::kafka::KickPlayerEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
    auto sessionIt = tlsSessionManager.sessions().find(event.session_id());
    if (sessionIt == tlsSessionManager.sessions().end()) {
        LOG_DEBUG << "KickPlayer ignored, session not found. session_id=" << event.session_id();
        return;
    }

    auto& conn = sessionIt->second.conn;
    if (conn && conn->connected()) {
        conn->shutdown();
        LOG_INFO << "KickPlayer applied. session_id=" << event.session_id();
        return;
    }

    // Defensive cleanup for stale entries without a live TCP connection.
    tlsSessionManager.sessions().erase(sessionIt);
    LOG_INFO << "KickPlayer cleaned stale session entry. session_id=" << event.session_id();
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
    auto sessionIt = tlsSessionManager.sessions().find(event.session_id());
    if (sessionIt == tlsSessionManager.sessions().end()) {
        LOG_DEBUG << "LeaseExpired ignored, session not found. session_id=" << event.session_id();
        return;
    }

    if (event.player_id() != 0 && sessionIt->second.playerId != 0 && sessionIt->second.playerId != event.player_id()) {
        LOG_WARN << "LeaseExpired ignored due to player mismatch. session_id=" << event.session_id()
            << ", session_player_id=" << sessionIt->second.playerId
            << ", event_player_id=" << event.player_id();
        return;
    }

    auto& conn = sessionIt->second.conn;
    if (conn && conn->connected()) {
        conn->shutdown();
        LOG_INFO << "LeaseExpired disconnected live session. session_id=" << event.session_id();
        return;
    }

    tlsSessionManager.sessions().erase(sessionIt);
    LOG_INFO << "LeaseExpired cleaned stale session. session_id=" << event.session_id();
///<<< END WRITING YOUR CODE
}
void GateEventHandler::BindSessionEventHandler(const contracts::kafka::BindSessionEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
    auto& sessions = tlsSessionManager.sessions();
    auto existingIt = sessions.find(event.session_id());

    if (existingIt != sessions.end()) {
        // Session already exists (e.g. reconnect on same gate) — update in place, keep conn & nodeIds.
        existingIt->second.playerId = event.player_id();
        existingIt->second.sessionVersion = event.session_version();
    } else {
        // New session entry.
        SessionInfo info;
        info.playerId = event.player_id();
        info.sessionVersion = event.session_version();
        sessions[event.session_id()] = info;
        existingIt = sessions.find(event.session_id());
    }

    LOG_INFO << "BindSession applied. session_id=" << event.session_id()
        << ", player_id=" << event.player_id()
        << ", session_version=" << event.session_version()
        << ", enter_gs_type=" << event.enter_gs_type();

    // Forward login notification to Scene if scene node is already assigned.
    const auto sceneNodeId = existingIt->second.GetNodeId(SceneNodeService);
    if (sceneNodeId != SessionInfo::kInvalidEntityId && event.enter_gs_type() != 0) {
        ForwardLoginToScene(event.session_id(), event.enter_gs_type(), sceneNodeId);
    } else if (event.enter_gs_type() != 0) {
        // Scene not yet assigned (e.g. FirstLogin) — store for deferred forwarding.
        existingIt->second.pendingEnterGsType = event.enter_gs_type();
    }
///<<< END WRITING YOUR CODE
}
