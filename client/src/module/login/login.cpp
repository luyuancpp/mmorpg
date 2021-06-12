#include "login.h"

void LoginModule::ReadyGo()
{
    LoginRequest request;
    request.set_account("luhailong11");
    request.set_password("lhl.2021");
    codec_.send(conn_, request);
}

void LoginModule::CreatePlayer()
{
    CreatePlayerRequest request;
    codec_.send(conn_, request);
}

void LoginModule::EnterGame(uint64_t player_id)
{
    EnterGameRequest request;
    request.set_player_id(player_id);
    codec_.send(conn_, request);
}
