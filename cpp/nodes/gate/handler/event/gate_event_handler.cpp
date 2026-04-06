#include "gate_event_handler.h"
#include "thread_context/dispatcher_manager.h"

///<<< BEGIN WRITING YOUR CODE
#include <session/manager/session_manager.h>
#include "muduo/base/Logging.h"
#include "proto/common/base/node.pb.h"
#include "proto/scene/player_lifecycle.pb.h"
#include "rpc/service_metadata/player_lifecycle_service_metadata.h"
#include "rpc/service_metadata/scene_service_metadata.h"
#include "thread_context/node_context_manager.h"
#include "network/rpc_session.h"
#include "node/system/node/node_util.h"

// Forward login notification from Gate to Scene via gRPC.
static void ForwardLoginToScene(uint64_t sessionId, uint32_t enterGsType, uint64_t sceneNodeId)
{
    if (enterGsType == 0)
        return; // LOGIN_NONE, nothing to forward

    auto &sceneRegistry = tlsNodeContextManager.GetRegistry(SceneNodeService);
    entt::entity sceneEntity{sceneNodeId};
    if (!sceneRegistry.valid(sceneEntity))
    {
        LOG_ERROR << "ForwardLoginToScene: scene node entity invalid, scene_node_id=" << sceneNodeId;
        return;
    }

    const auto *rpcSession = sceneRegistry.try_get<RpcSession>(sceneEntity);
    if (!rpcSession)
    {
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
///<<< END WRITING YOUR CODE
}
void GateEventHandler::KickPlayerEventHandler(const contracts::kafka::KickPlayerEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
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
///<<< END WRITING YOUR CODE
}
