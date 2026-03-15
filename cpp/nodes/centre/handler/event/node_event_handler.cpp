#include "node_event_handler.h"
#include "threading/dispatcher_manager.h"

void NodeEventHandler::Register()
{
    dispatcher.sink<OnNodeAddPbEvent>().connect<&NodeEventHandler::OnNodeAddPbEventHandler>();
    dispatcher.sink<OnNodeRemovePbEvent>().connect<&NodeEventHandler::OnNodeRemovePbEventHandler>();
    dispatcher.sink<ConnectToNodePbEvent>().connect<&NodeEventHandler::ConnectToNodePbEventHandler>();
    dispatcher.sink<OnNodeConnectedPbEvent>().connect<&NodeEventHandler::OnNodeConnectedPbEventHandler>();
}

void NodeEventHandler::UnRegister()
{
    dispatcher.sink<OnNodeAddPbEvent>().disconnect<&NodeEventHandler::OnNodeAddPbEventHandler>();
    dispatcher.sink<OnNodeRemovePbEvent>().disconnect<&NodeEventHandler::OnNodeRemovePbEventHandler>();
    dispatcher.sink<ConnectToNodePbEvent>().disconnect<&NodeEventHandler::ConnectToNodePbEventHandler>();
    dispatcher.sink<OnNodeConnectedPbEvent>().disconnect<&NodeEventHandler::OnNodeConnectedPbEventHandler>();
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
