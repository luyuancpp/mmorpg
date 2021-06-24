#include "login_state_machine.h"

#include "src/return_code/return_notice_code.h"

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
        set_state(E_LOGIN_STATE_NONE);
    }

    void LoginStateMachine::set_state(int32_t state_enum)
    {
        current_state_ = state_list_[state_enum];
    }

    int32_t LoginStateMachine::Login()
{
        return current_state_->Login();
    }

    int32_t LoginStateMachine::CreatePlayer()
    {
        return current_state_->CreatePlayer();
    }

    int32_t LoginStateMachine::EnterGame()
    {
        return current_state_->EnterGame();
    }

    void LoginStateMachine::WaitingEnterGame()
    {
        current_state_->WaitingEnterGame();
    }

    void LoginStateMachine::NoPlayer()
    {
        current_state_->NoPlayer();
    }

    void LoginStateMachine::FullPlayer()
    {
        current_state_->FullPlayer();
    }

    void LoginStateMachine::Playing()
{
        current_state_->Playing();
    }

    void LoginStateMachine::receive(const LoginESSetState& s)
    {
        set_state(s.state_id_);
    }

}//namespace common