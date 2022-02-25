#include "login_state.h"

#include "login_state_machine.h"
#include "login_state_concrete.h"

namespace common
{

    uint32_t IAccountState::LogoutAccount()
    {
        login_machine_.set_state(E_LOGIN_NONE);
        return RET_OK;
    }

    void IAccountState::WaitingEnterGame()
    {
        login_machine_.set_state(E_LOGIN_WAITING_ENTER_GAME);
    }

    void IAccountState::OnFullPlayer()
    {
        login_machine_.set_state(E_LOGIN_ACCOUNT_FULL_PLAYER); 
    }

}//namespace common

