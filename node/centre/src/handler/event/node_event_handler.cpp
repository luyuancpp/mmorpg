#include "node_event_handler.h"
#include "proto/logic/event/node_event.pb.h"
#include "thread_local/storage.h"

///<<< BEGIN WRITING YOUR CODE
#include "network/rpc_session.h"
///<<< END WRITING YOUR CODE


void NodeEventHandler::Register()
{
    tls.dispatcher.sink<OnSceneNodeAddPbEvent>().connect<&NodeEventHandler::OnSceneNodeAddPbEventHandler>();
    tls.dispatcher.sink<OnSceneNodeRemovePbEvent>().connect<&NodeEventHandler::OnSceneNodeRemovePbEventHandler>();
    tls.dispatcher.sink<OnNodeAddPbEvent>().connect<&NodeEventHandler::OnNodeAddPbEventHandler>();
    tls.dispatcher.sink<OnNodeRemovePbEvent>().connect<&NodeEventHandler::OnNodeRemovePbEventHandler>();
    tls.dispatcher.sink<ConnectToNodePbEvent>().connect<&NodeEventHandler::ConnectToNodePbEventHandler>();
    tls.dispatcher.sink<OnNodeConnectedPbEvent>().connect<&NodeEventHandler::OnNodeConnectedPbEventHandler>();
}

void NodeEventHandler::UnRegister()
{
    tls.dispatcher.sink<OnSceneNodeAddPbEvent>().disconnect<&NodeEventHandler::OnSceneNodeAddPbEventHandler>();
    tls.dispatcher.sink<OnSceneNodeRemovePbEvent>().disconnect<&NodeEventHandler::OnSceneNodeRemovePbEventHandler>();
    tls.dispatcher.sink<OnNodeAddPbEvent>().disconnect<&NodeEventHandler::OnNodeAddPbEventHandler>();
    tls.dispatcher.sink<OnNodeRemovePbEvent>().disconnect<&NodeEventHandler::OnNodeRemovePbEventHandler>();
    tls.dispatcher.sink<ConnectToNodePbEvent>().disconnect<&NodeEventHandler::ConnectToNodePbEventHandler>();
    tls.dispatcher.sink<OnNodeConnectedPbEvent>().disconnect<&NodeEventHandler::OnNodeConnectedPbEventHandler>();
}

void NodeEventHandler::OnSceneNodeAddPbEventHandler(const OnSceneNodeAddPbEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
		// Log that we have received a RegisterGame request
	LOG_INFO << "Received RegisterGame request.";




///<<< END WRITING YOUR CODE

}

void NodeEventHandler::OnSceneNodeRemovePbEventHandler(const OnSceneNodeRemovePbEvent& event)
{
///<<< BEGIN WRITING YOUR CODE

///<<< END WRITING YOUR CODE

}

void NodeEventHandler::OnNodeAddPbEventHandler(const OnNodeAddPbEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void NodeEventHandler::OnNodeRemovePbEventHandler(const OnNodeRemovePbEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void NodeEventHandler::ConnectToNodePbEventHandler(const ConnectToNodePbEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void NodeEventHandler::OnNodeConnectedPbEventHandler(const OnNodeConnectedPbEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}
