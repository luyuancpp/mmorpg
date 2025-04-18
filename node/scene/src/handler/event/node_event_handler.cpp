#include "node_event_handler.h"
#include "proto/logic/event/node_event.pb.h"
#include "thread_local/storage.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE


void NodeEventHandler::Register()
{
    tls.dispatcher.sink<OnSceneNodeAddPbEvent>().connect<&NodeEventHandler::OnSceneNodeAddPbEventHandler>();
}

void NodeEventHandler::UnRegister()
{
    tls.dispatcher.sink<OnSceneNodeAddPbEvent>().disconnect<&NodeEventHandler::OnSceneNodeAddPbEventHandler>();
}

void NodeEventHandler::OnSceneNodeAddPbEventHandler(const OnSceneNodeAddPbEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}
