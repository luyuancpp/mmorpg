#include "login_state_machine.h"

#include "src/return_code/error_code.h"

namespace common
{
    LoginStateMachine::LoginStateMachine()
        : emp_(EventManager::New())
    {
        for (uint32_t i = 0; i < E_LOGIN_STATE_MAX; ++i)
        {
            state_list_[i] = ILoginState::CreateState(i, emp_);
        }
        emp_->subscribe<LoginESSetState>(*this);
        set_state(E_LOGIN_NONE);
    }

    void LoginStateMachine::set_state(uint32_t state_enum)
    {
        current_state_ = state_list_[state_enum];
    }

    uint32_t LoginStateMachine::Login()
{
        return current_state_->Login();
    }

    uint32_t LoginStateMachine::Logout()
    {
        return current_state_->Logout();
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

    void LoginStateMachine::receive(const LoginESSetState& s)
    {
        set_state(s.state_id_);
    }

}//namespace common