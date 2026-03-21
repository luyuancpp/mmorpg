#include "player_event_handler.h"
#include "thread_context/dispatcher_manager.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void PlayerEventHandler::Register()
{
    dispatcher.sink<contracts::kafka::PlayerLifecycleCommand>().connect<&PlayerEventHandler::PlayerLifecycleCommandHandler>();
}

void PlayerEventHandler::UnRegister()
{
    dispatcher.sink<contracts::kafka::PlayerLifecycleCommand>().disconnect<&PlayerEventHandler::PlayerLifecycleCommandHandler>();
}
void PlayerEventHandler::PlayerLifecycleCommandHandler(const contracts::kafka::PlayerLifecycleCommand& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
