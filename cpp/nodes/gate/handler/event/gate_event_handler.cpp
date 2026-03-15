#include "gate_event_handler.h"
#include "threading/dispatcher_manager.h"

///<<< BEGIN WRITING YOUR CODE
#include <session/manager/session_manager.h>
#include "muduo/base/Logging.h"
#include "proto/common/base/node.pb.h"
///<<< END WRITING YOUR CODE
void GateEventHandler::Register()
{
    dispatcher.sink<contracts::kafka::RoutePlayerEvent>().connect<&GateEventHandler::RoutePlayerEventHandler>();
    dispatcher.sink<contracts::kafka::KickPlayerEvent>().connect<&GateEventHandler::KickPlayerEventHandler>();
    dispatcher.sink<contracts::kafka::BindSessionEvent>().connect<&GateEventHandler::BindSessionEventHandler>();
    dispatcher.sink<contracts::kafka::PlayerLeaseExpiredEvent>().connect<&GateEventHandler::PlayerLeaseExpiredEventHandler>();
}

void GateEventHandler::UnRegister()
{
    dispatcher.sink<contracts::kafka::RoutePlayerEvent>().disconnect<&GateEventHandler::RoutePlayerEventHandler>();
    dispatcher.sink<contracts::kafka::KickPlayerEvent>().disconnect<&GateEventHandler::KickPlayerEventHandler>();
    dispatcher.sink<contracts::kafka::BindSessionEvent>().disconnect<&GateEventHandler::BindSessionEventHandler>();
    dispatcher.sink<contracts::kafka::PlayerLeaseExpiredEvent>().disconnect<&GateEventHandler::PlayerLeaseExpiredEventHandler>();
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

void GateEventHandler::BindSessionEventHandler(const contracts::kafka::BindSessionEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
    SessionInfo info;
    info.playerId = event.player_id();
    info.sessionVersion = event.session_version();
    tlsSessionManager.sessions()[event.session_id()] = info;

    LOG_INFO << "BindSession applied. session_id=" << event.session_id()
        << ", player_id=" << event.player_id()
        << ", session_version=" << event.session_version();
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
