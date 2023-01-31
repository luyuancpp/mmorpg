#pragma once

#include "src/comp/player_list.h"

class ControllerThreadLocalStorage
{
public:
	PlayerList& player_list() { return player_list_; }
private:
	PlayerList player_list_;
};

extern thread_local ControllerThreadLocalStorage controller_tls;

