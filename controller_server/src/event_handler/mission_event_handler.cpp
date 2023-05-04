#include "mission_event_handler.h"
#include "event_proto/mission_event.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void MissionEventHandler::Register(entt::dispatcher& dispatcher)
{
dispatcher.sink<AcceptMissionEvent>().connect<&MissionEventHandler::MissionEventHandler::Receive0>();
dispatcher.sink<MissionConditionEvent>().connect<&MissionEventHandler::MissionEventHandler::Receive1>();
dispatcher.sink<OnAcceptedMissionEvent>().connect<&MissionEventHandler::MissionEventHandler::Receive2>();
dispatcher.sink<OnMissionAwardEvent>().connect<&MissionEventHandler::MissionEventHandler::Receive3>();
}

void MissionEventHandler::UnRegister(entt::dispatcher& dispatcher)
{
dispatcher.sink<AcceptMissionEvent>().disconnect<&MissionEventHandler::MissionEventHandler::Receive0>();
dispatcher.sink<MissionConditionEvent>().disconnect<&MissionEventHandler::MissionEventHandler::Receive1>();
dispatcher.sink<OnAcceptedMissionEvent>().disconnect<&MissionEventHandler::MissionEventHandler::Receive2>();
dispatcher.sink<OnMissionAwardEvent>().disconnect<&MissionEventHandler::MissionEventHandler::Receive3>();
}

void MissionEventHandler::Receive0(const AcceptMissionEvent& event_obj)
{
///<<< BEGIN WRITING YOUR CODE
///     test
///<<< END WRITING YOUR CODE
}

void MissionEventHandler::Receive1(const MissionConditionEvent& event_obj)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void MissionEventHandler::Receive2(const OnAcceptedMissionEvent& event_obj)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void MissionEventHandler::Receive3(const OnMissionAwardEvent& event_obj)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

