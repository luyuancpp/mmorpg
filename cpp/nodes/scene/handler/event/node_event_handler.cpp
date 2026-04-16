#include "node_event_handler.h"
#include "thread_context/dispatcher_manager.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void NodeEventHandler::Register()
{
    tlsEcs.dispatcher.sink<OnNodeAddEvent>().connect<&NodeEventHandler::OnNodeAddEventHandler>();
    tlsEcs.dispatcher.sink<OnNodeRemoveEvent>().connect<&NodeEventHandler::OnNodeRemoveEventHandler>();
    tlsEcs.dispatcher.sink<ConnectToNodeEvent>().connect<&NodeEventHandler::ConnectToNodeEventHandler>();
    tlsEcs.dispatcher.sink<OnNodeConnectedEvent>().connect<&NodeEventHandler::OnNodeConnectedEventHandler>();
}

void NodeEventHandler::UnRegister()
{
    tlsEcs.dispatcher.sink<OnNodeAddEvent>().disconnect<&NodeEventHandler::OnNodeAddEventHandler>();
    tlsEcs.dispatcher.sink<OnNodeRemoveEvent>().disconnect<&NodeEventHandler::OnNodeRemoveEventHandler>();
    tlsEcs.dispatcher.sink<ConnectToNodeEvent>().disconnect<&NodeEventHandler::ConnectToNodeEventHandler>();
    tlsEcs.dispatcher.sink<OnNodeConnectedEvent>().disconnect<&NodeEventHandler::OnNodeConnectedEventHandler>();
}
void NodeEventHandler::OnNodeAddEventHandler(const OnNodeAddEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
void NodeEventHandler::OnNodeRemoveEventHandler(const OnNodeRemoveEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
void NodeEventHandler::ConnectToNodeEventHandler(const ConnectToNodeEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
void NodeEventHandler::OnNodeConnectedEventHandler(const OnNodeConnectedEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
