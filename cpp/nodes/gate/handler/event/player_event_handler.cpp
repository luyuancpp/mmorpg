#include "player_event_handler.h"
#include "thread_context/ecs_context.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void PlayerEventHandler::Register()
{
    tlsEcs.dispatcher.sink<contracts::kafka::PlayerLifecycleCommand>().connect<&PlayerEventHandler::PlayerLifecycleCommandHandler>();
}

void PlayerEventHandler::UnRegister()
{
    tlsEcs.dispatcher.sink<contracts::kafka::PlayerLifecycleCommand>().disconnect<&PlayerEventHandler::PlayerLifecycleCommandHandler>();
}
void PlayerEventHandler::PlayerLifecycleCommandHandler(const contracts::kafka::PlayerLifecycleCommand& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
