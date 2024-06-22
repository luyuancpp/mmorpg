#include "server_event_handler.h"
#include "event_proto/server_event.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void ServerEventHandler::Register()
{
		tls.dispatcher.sink<OnConnect2Centre>().connect<&ServerEventHandler::OnConnect2CentreHandler>();
		tls.dispatcher.sink<OnConnect2Game>().connect<&ServerEventHandler::OnConnect2GameHandler>();
		tls.dispatcher.sink<OnConnect2Gate>().connect<&ServerEventHandler::OnConnect2GateHandler>();
		tls.dispatcher.sink<OnConnect2Login>().connect<&ServerEventHandler::OnConnect2LoginHandler>();
}

void ServerEventHandler::UnRegister()
{
		tls.dispatcher.sink<OnConnect2Centre>().disconnect<&ServerEventHandler::OnConnect2CentreHandler>();
		tls.dispatcher.sink<OnConnect2Game>().disconnect<&ServerEventHandler::OnConnect2GameHandler>();
		tls.dispatcher.sink<OnConnect2Gate>().disconnect<&ServerEventHandler::OnConnect2GateHandler>();
		tls.dispatcher.sink<OnConnect2Login>().disconnect<&ServerEventHandler::OnConnect2LoginHandler>();
}

void ServerEventHandler::OnConnect2CentreHandler(const OnConnect2Centre& message)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerEventHandler::OnConnect2GameHandler(const OnConnect2Game& message)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerEventHandler::OnConnect2GateHandler(const OnConnect2Gate& message)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerEventHandler::OnConnect2LoginHandler(const OnConnect2Login& message)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

