#include "server_event_handler.h"
#include "thread_context/dispatcher_manager.h"

///<<< BEGIN WRITING YOUR CODE
#include "node/system/node/node.h"
///<<< END WRITING YOUR CODE
void ServerEventHandler::Register()
{
    tlsEcs.dispatcher.sink<OnConnect2CentreEvent>().connect<&ServerEventHandler::OnConnect2CentreEventHandler>();
    tlsEcs.dispatcher.sink<OnConnect2Login>().connect<&ServerEventHandler::OnConnect2LoginHandler>();
    tlsEcs.dispatcher.sink<OnServerStart>().connect<&ServerEventHandler::OnServerStartHandler>();
}

void ServerEventHandler::UnRegister()
{
    tlsEcs.dispatcher.sink<OnConnect2CentreEvent>().disconnect<&ServerEventHandler::OnConnect2CentreEventHandler>();
    tlsEcs.dispatcher.sink<OnConnect2Login>().disconnect<&ServerEventHandler::OnConnect2LoginHandler>();
    tlsEcs.dispatcher.sink<OnServerStart>().disconnect<&ServerEventHandler::OnServerStartHandler>();
}
void ServerEventHandler::OnConnect2CentreEventHandler(const OnConnect2CentreEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
void ServerEventHandler::OnConnect2LoginHandler(const OnConnect2Login& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
void ServerEventHandler::OnServerStartHandler(const OnServerStart& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
