#include "match_event_handler.h"
#include "thread_context/ecs_context.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void MatchEventHandler::Register()
{
    tlsEcs.dispatcher.sink<contracts::kafka::BattleResultTeam>().connect<&MatchEventHandler::BattleResultTeamHandler>();
    tlsEcs.dispatcher.sink<contracts::kafka::BattleResultEvent>().connect<&MatchEventHandler::BattleResultEventHandler>();
    tlsEcs.dispatcher.sink<contracts::kafka::PlayerRatingSnapshotEvent>().connect<&MatchEventHandler::PlayerRatingSnapshotEventHandler>();
}

void MatchEventHandler::UnRegister()
{
    tlsEcs.dispatcher.sink<contracts::kafka::BattleResultTeam>().disconnect<&MatchEventHandler::BattleResultTeamHandler>();
    tlsEcs.dispatcher.sink<contracts::kafka::BattleResultEvent>().disconnect<&MatchEventHandler::BattleResultEventHandler>();
    tlsEcs.dispatcher.sink<contracts::kafka::PlayerRatingSnapshotEvent>().disconnect<&MatchEventHandler::PlayerRatingSnapshotEventHandler>();
}
void MatchEventHandler::BattleResultTeamHandler(const contracts::kafka::BattleResultTeam& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
void MatchEventHandler::BattleResultEventHandler(const contracts::kafka::BattleResultEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
void MatchEventHandler::PlayerRatingSnapshotEventHandler(const contracts::kafka::PlayerRatingSnapshotEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
