#include "login_state.h"

#include "login_state_machine.h"
#include "login_state_concrete.h"

uint32_t IAccountState::LogoutAccount()
{
    login_machine_.set_state(kLoginNone);
    return kRetOK;
}

void IAccountState::WaitingEnterGame()
{
    login_machine_.set_state(kLoginWatingEnterGame);
}

void IAccountState::OnFullPlayer()
{
    login_machine_.set_state(kLoginAccountFullPlayer); 
}


