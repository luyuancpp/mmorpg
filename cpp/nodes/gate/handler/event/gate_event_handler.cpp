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
}

void GateEventHandler::UnRegister()
{
    dispatcher.sink<contracts::kafka::RoutePlayerEvent>().disconnect<&GateEventHandler::RoutePlayerEventHandler>();
    dispatcher.sink<contracts::kafka::KickPlayerEvent>().disconnect<&GateEventHandler::KickPlayerEventHandler>();
}
void GateEventHandler::RoutePlayerEventHandler(const RoutePlayerEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
void GateEventHandler::KickPlayerEventHandler(const KickPlayerEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
