#include "server_event_handler.h"
#include "event_proto/server_event.pb.h"
#include "thread_local/storage.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void ServerEventHandler::Register()
{
		tls.dispatcher.sink<OnConnect2Centre>().connect<&ServerEventHandler::OnConnect2CentreHandler>();
		tls.dispatcher.sink<OnConnect2Game>().connect<&ServerEventHandler::OnConnect2GameHandler>();
		tls.dispatcher.sink<OnConnect2Gate>().connect<&ServerEventHandler::OnConnect2GateHandler>();
		tls.dispatcher.sink<OnConnect2Login>().connect<&ServerEventHandler::OnConnect2LoginHandler>();
		tls.dispatcher.sink<OnServerStart>().connect<&ServerEventHandler::OnServerStartHandler>();
}

void ServerEventHandler::UnRegister()
{
		tls.dispatcher.sink<OnConnect2Centre>().disconnect<&ServerEventHandler::OnConnect2CentreHandler>();
		tls.dispatcher.sink<OnConnect2Game>().disconnect<&ServerEventHandler::OnConnect2GameHandler>();
		tls.dispatcher.sink<OnConnect2Gate>().disconnect<&ServerEventHandler::OnConnect2GateHandler>();
		tls.dispatcher.sink<OnConnect2Login>().disconnect<&ServerEventHandler::OnConnect2LoginHandler>();
		tls.dispatcher.sink<OnServerStart>().disconnect<&ServerEventHandler::OnServerStartHandler>();
}

void ServerEventHandler::OnConnect2CentreHandler(const OnConnect2Centre& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerEventHandler::OnConnect2GameHandler(const OnConnect2Game& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerEventHandler::OnConnect2GateHandler(const OnConnect2Gate& event)
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

