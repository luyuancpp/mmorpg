#include "server_event_handler.h"
#include "thread_context/dispatcher_manager.h"

void ServerEventHandler::Register()
{
    tlsEcs.dispatcher.sink<OnConnect2CentrePbEvent>().connect<&ServerEventHandler::OnConnect2CentrePbEventHandler>();
    tlsEcs.dispatcher.sink<OnConnect2Login>().connect<&ServerEventHandler::OnConnect2LoginHandler>();
    tlsEcs.dispatcher.sink<OnServerStart>().connect<&ServerEventHandler::OnServerStartHandler>();
}

void ServerEventHandler::UnRegister()
{
    tlsEcs.dispatcher.sink<OnConnect2CentrePbEvent>().disconnect<&ServerEventHandler::OnConnect2CentrePbEventHandler>();
    tlsEcs.dispatcher.sink<OnConnect2Login>().disconnect<&ServerEventHandler::OnConnect2LoginHandler>();
    tlsEcs.dispatcher.sink<OnServerStart>().disconnect<&ServerEventHandler::OnServerStartHandler>();
}
void ServerEventHandler::OnConnect2CentrePbEventHandler(const OnConnect2CentrePbEvent& event)
{

}
void ServerEventHandler::OnConnect2LoginHandler(const OnConnect2Login& event)
{

}
void ServerEventHandler::OnServerStartHandler(const OnServerStart& event)
{

}

