#include "mission_event_handler.h"
#include "event_proto/mission_event.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void MissionEventHandler::Register()
{
		tls.dispatcher.sink<AcceptMissionEvent>().connect<&MissionEventHandler::AcceptMissionEventHandler>();
		tls.dispatcher.sink<MissionConditionEvent>().connect<&MissionEventHandler::MissionConditionEventHandler>();
		tls.dispatcher.sink<OnAcceptedMissionEvent>().connect<&MissionEventHandler::OnAcceptedMissionEventHandler>();
		tls.dispatcher.sink<OnMissionAwardEvent>().connect<&MissionEventHandler::OnMissionAwardEventHandler>();
}

void MissionEventHandler::UnRegister()
{
		tls.dispatcher.sink<AcceptMissionEvent>().disconnect<&MissionEventHandler::AcceptMissionEventHandler>();
		tls.dispatcher.sink<MissionConditionEvent>().disconnect<&MissionEventHandler::MissionConditionEventHandler>();
		tls.dispatcher.sink<OnAcceptedMissionEvent>().disconnect<&MissionEventHandler::OnAcceptedMissionEventHandler>();
		tls.dispatcher.sink<OnMissionAwardEvent>().disconnect<&MissionEventHandler::OnMissionAwardEventHandler>();
}

void MissionEventHandler::AcceptMissionEventHandler(const AcceptMissionEvent& message)
{
///<<< BEGIN WRITING YOUR CODE
///     test
///<<< END WRITING YOUR CODE
}

void MissionEventHandler::MissionConditionEventHandler(const MissionConditionEvent& message)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void MissionEventHandler::OnAcceptedMissionEventHandler(const OnAcceptedMissionEvent& message)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void MissionEventHandler::OnMissionAwardEventHandler(const OnMissionAwardEvent& message)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

