#include "actor_event_handler.h"
#include "thread_context/ecs_context.h"

///<<< BEGIN WRITING YOUR CODE

///<<< END WRITING YOUR CODE
void ActorEventHandler::Register()
{
    tlsEcs.dispatcher.sink<InitializeActorCompsEvent>().connect<&ActorEventHandler::InitializeActorCompsEventHandler>();
    tlsEcs.dispatcher.sink<InterruptCurrentStateEvent>().connect<&ActorEventHandler::InterruptCurrentStateEventHandler>();
}

void ActorEventHandler::UnRegister()
{
    tlsEcs.dispatcher.sink<InitializeActorCompsEvent>().disconnect<&ActorEventHandler::InitializeActorCompsEventHandler>();
    tlsEcs.dispatcher.sink<InterruptCurrentStateEvent>().disconnect<&ActorEventHandler::InterruptCurrentStateEventHandler>();
}
void ActorEventHandler::InitializeActorCompsEventHandler(const InitializeActorCompsEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
void ActorEventHandler::InterruptCurrentStateEventHandler(const InterruptCurrentStateEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
