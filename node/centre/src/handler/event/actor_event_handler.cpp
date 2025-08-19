#include "actor_event_handler.h"
#include "proto/logic/event/actor_event.pb.h"
#include "thread_local/storage.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE


void ActorEventHandler::Register()
{
    tls.dispatcher.sink<InitializeActorComponentsEvent>().connect<&ActorEventHandler::InitializeActorComponentsEventHandler>();
    tls.dispatcher.sink<InterruptCurrentStatePbEvent>().connect<&ActorEventHandler::InterruptCurrentStatePbEventHandler>();
}

void ActorEventHandler::UnRegister()
{
    tls.dispatcher.sink<InitializeActorComponentsEvent>().disconnect<&ActorEventHandler::InitializeActorComponentsEventHandler>();
    tls.dispatcher.sink<InterruptCurrentStatePbEvent>().disconnect<&ActorEventHandler::InterruptCurrentStatePbEventHandler>();
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
