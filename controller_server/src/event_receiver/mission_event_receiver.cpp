#include "mission_event_receiver.h"
#include "event_proto/mission_event.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void MissionEventReceiver::Register(entt::dispatcher& dispatcher)
{
dispatcher.sink<AcceptMissionEvent>().connect<&MissionEventReceiver::MissionEventReceiver::Receive0>();
dispatcher.sink<MissionConditionEvent>().connect<&MissionEventReceiver::MissionEventReceiver::Receive1>();
dispatcher.sink<OnAcceptedMissionEvent>().connect<&MissionEventReceiver::MissionEventReceiver::Receive2>();
dispatcher.sink<OnMissionAwardEvent>().connect<&MissionEventReceiver::MissionEventReceiver::Receive3>();
}

void MissionEventReceiver::UnRegister(entt::dispatcher& dispatcher)
{
dispatcher.sink<AcceptMissionEvent>().disconnect<&MissionEventReceiver::MissionEventReceiver::Receive0>();
dispatcher.sink<MissionConditionEvent>().disconnect<&MissionEventReceiver::MissionEventReceiver::Receive1>();
dispatcher.sink<OnAcceptedMissionEvent>().disconnect<&MissionEventReceiver::MissionEventReceiver::Receive2>();
dispatcher.sink<OnMissionAwardEvent>().disconnect<&MissionEventReceiver::MissionEventReceiver::Receive3>();
}

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

