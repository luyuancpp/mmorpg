#include "server_event_handler.h"
#include "proto/logic/event/server_event.pb.h"
#include "thread_local/storage.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE


void ServerEventHandler::Register()
{
    tls.dispatcher.sink<OnConnect2CentrePbEvent>().connect<&ServerEventHandler::OnConnect2CentrePbEventHandler>();
    tls.dispatcher.sink<OnConnect2Login>().connect<&ServerEventHandler::OnConnect2LoginHandler>();
    tls.dispatcher.sink<OnServerStart>().connect<&ServerEventHandler::OnServerStartHandler>();
}

void ServerEventHandler::UnRegister()
{
    tls.dispatcher.sink<OnConnect2CentrePbEvent>().disconnect<&ServerEventHandler::OnConnect2CentrePbEventHandler>();
    tls.dispatcher.sink<OnConnect2Login>().disconnect<&ServerEventHandler::OnConnect2LoginHandler>();
    tls.dispatcher.sink<OnServerStart>().disconnect<&ServerEventHandler::OnServerStartHandler>();
}
void ServerEventHandler::OnConnect2CentrePbEventHandler(const OnConnect2CentrePbEvent& event)
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
