#include "login.h"

void LoginModule::ReadyGo()
{
    LoginRequest request;
    static uint32_t i = 100;
    request.set_account(std::string("luhailong") + std::to_string(++i));
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
