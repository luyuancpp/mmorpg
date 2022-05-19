#include "login_state_machine.h"

#include "src/game_logic/tips_id.h"
#include "login_state_concrete.h"


LoginStateMachine::LoginStateMachine()
{
    state_list_[kLoginNone] = std::make_shared<NoneState>(*this);
    state_list_[kLoginAccountLogining] = std::make_shared<LoginState>(*this);
    state_list_[kLoginAcccountCreatePlayer] = std::make_shared<CreatePlayerState>(*this);
    state_list_[kLoginAccountEnterGame] = std::make_shared<EnterGameState>(*this);
    state_list_[kLoignAccountPling] = std::make_shared<PlayingState>(*this);
    state_list_[kLoginWatingEnterGame] = std::make_shared<WaitingEnterGameState>(*this);
    state_list_[kLoginAccountNoPlayer] = std::make_shared<EmptyPlayerState>(*this);
    state_list_[kLoginAccountFullPlayer] = std::make_shared<FullPlayerState>(*this);
    set_state(kLoginNone);
}

void LoginStateMachine::set_state(uint32_t state_enum)
{
    current_state_ = state_list_[state_enum];
}

uint32_t LoginStateMachine::Login()
{
    return current_state_->LoginAccount();
}

uint32_t LoginStateMachine::CreatePlayer()
{
    return current_state_->CreatePlayer();
}

uint32_t LoginStateMachine::EnterGame()
{
    return current_state_->EnterGame();
}

void LoginStateMachine::WaitingEnterGame()
{
    current_state_->WaitingEnterGame();
}

void LoginStateMachine::OnEmptyPlayer()
{
    current_state_->OnEmptyPlayer();
}

void LoginStateMachine::OnFullPlayer()
{
    current_state_->OnFullPlayer();
}

void LoginStateMachine::OnPlaying()
{
    current_state_->OnPlaying();
}
