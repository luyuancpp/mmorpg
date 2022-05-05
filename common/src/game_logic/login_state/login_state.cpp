#include "login_state.h"

#include "login_state_machine.h"
#include "login_state_concrete.h"

void IAccountState::WaitingEnterGame()
{
    login_machine_.set_state(kLoginWatingEnterGame);
}

void IAccountState::OnFullPlayer()
{
    login_machine_.set_state(kLoginAccountFullPlayer); 
}


