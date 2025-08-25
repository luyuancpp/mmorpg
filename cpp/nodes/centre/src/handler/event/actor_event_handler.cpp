#include "actor_event_handler.h"
#include "proto/logic/event/actor_event.pb.h"
#include "threading/dispatcher_manager.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE


void ActorEventHandler::Register()
{
    dispatcher.sink<InitializeActorComponentsEvent>().connect<&ActorEventHandler::InitializeActorComponentsEventHandler>();
    dispatcher.sink<InterruptCurrentStatePbEvent>().connect<&ActorEventHandler::InterruptCurrentStatePbEventHandler>();
}

void ActorEventHandler::UnRegister()
{
    dispatcher.sink<InitializeActorComponentsEvent>().disconnect<&ActorEventHandler::InitializeActorComponentsEventHandler>();
    dispatcher.sink<InterruptCurrentStatePbEvent>().disconnect<&ActorEventHandler::InterruptCurrentStatePbEventHandler>();
}
void ActorEventHandler::InitializeActorComponentsEventHandler(const InitializeActorComponentsEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
void ActorEventHandler::InterruptCurrentStatePbEventHandler(const InterruptCurrentStatePbEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
