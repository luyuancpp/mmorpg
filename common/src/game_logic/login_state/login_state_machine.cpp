#include "login_state_machine.h"

#include "src/return_code/error_code.h"
#include "login_state_concrete.h"

namespace common
{
    LoginStateMachine::LoginStateMachine()
    {
        state_list_[E_LOGIN_NONE] = std::make_shared<NoneState>(*this);
        state_list_[E_LOGIN_ACCOUNT_LOGIN] = std::make_shared<LoginState>(*this);
        state_list_[E_LOGIN_ACCOUNT_CREATE_PLAYER] = std::make_shared<CreatePlayerState>(*this);
        state_list_[E_LOGIN_ACCOUNT_ENTER_GAME] = std::make_shared<EnterGameState>(*this);
        state_list_[E_LGOIN_ACCOUNT_PLAYING] = std::make_shared<PlayingState>(*this);
        state_list_[E_LOGIN_WAITING_ENTER_GAME] = std::make_shared<WaitingEnterGameState>(*this);
        state_list_[E_LOGIN_ACCOUNT_NO_PLAYER] = std::make_shared<EmptyPlayerState>(*this);
        state_list_[E_LOGIN_ACCOUNT_FULL_PLAYER] = std::make_shared<FullPlayerState>(*this);
        set_state(E_LOGIN_NONE);
    }

    void LoginStateMachine::set_state(uint32_t state_enum)
    {
        current_state_ = state_list_[state_enum];
    }

    uint32_t LoginStateMachine::Login()
{
        return current_state_->LoginAccount();
    }

    uint32_t LoginStateMachine::Logout()
    {
        return current_state_->LogoutAccount();
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

}//namespace common