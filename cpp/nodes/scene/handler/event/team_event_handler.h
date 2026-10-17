#pragma once

#include "proto/common/event/team_event.pb.h"

class TeamEventHandler
{
public:
    static void PlayerTeamRefreshEventHandler(const PlayerTeamRefreshEvent& event);
};
