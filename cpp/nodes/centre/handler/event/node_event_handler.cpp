#include "node_event_handler.h"
#include "thread_context/dispatcher_manager.h"

void NodeEventHandler::Register()
{
    tlsEcs.dispatcher.sink<OnNodeAddPbEvent>().connect<&NodeEventHandler::OnNodeAddPbEventHandler>();
    tlsEcs.dispatcher.sink<OnNodeRemovePbEvent>().connect<&NodeEventHandler::OnNodeRemovePbEventHandler>();
    tlsEcs.dispatcher.sink<ConnectToNodePbEvent>().connect<&NodeEventHandler::ConnectToNodePbEventHandler>();
    tlsEcs.dispatcher.sink<OnNodeConnectedPbEvent>().connect<&NodeEventHandler::OnNodeConnectedPbEventHandler>();
}

void NodeEventHandler::UnRegister()
{
    tlsEcs.dispatcher.sink<OnNodeAddPbEvent>().disconnect<&NodeEventHandler::OnNodeAddPbEventHandler>();
    tlsEcs.dispatcher.sink<OnNodeRemovePbEvent>().disconnect<&NodeEventHandler::OnNodeRemovePbEventHandler>();
    tlsEcs.dispatcher.sink<ConnectToNodePbEvent>().disconnect<&NodeEventHandler::ConnectToNodePbEventHandler>();
    tlsEcs.dispatcher.sink<OnNodeConnectedPbEvent>().disconnect<&NodeEventHandler::OnNodeConnectedPbEventHandler>();
}
void NodeEventHandler::OnNodeAddPbEventHandler(const OnNodeAddPbEvent& event)
{

}
void NodeEventHandler::OnNodeRemovePbEventHandler(const OnNodeRemovePbEvent& event)
{

}
void NodeEventHandler::ConnectToNodePbEventHandler(const ConnectToNodePbEvent& event)
{

}
void NodeEventHandler::OnNodeConnectedPbEventHandler(const OnNodeConnectedPbEvent& event)
{

}

