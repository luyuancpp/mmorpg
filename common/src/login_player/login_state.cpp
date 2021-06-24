#include "login_state.h"

#include "login_state_concrete.h"

namespace common
{

    int32_t ILoginState::Processing()
    {
        return RET_OK;
    }

    ILoginState::StatePtr ILoginState::CreateState(int32_t state_enum, EventManagerPtr& emp)
    {
        StatePtr ptr;

        switch (state_enum)
        {
        case E_LOGIN_STATE_NONE:
        {
            ptr = std::make_shared<NoneState>(emp);
            break;
        }
        case E_LOGIN_STATE_LOGIN:
        {
            ptr = std::make_shared<LoginState>(emp);
            break;
        }
        case E_LOGIN_STATE_CREATE_PLAYER:
        {
            ptr = std::make_shared<CreatePlayerState>(emp);
            break;
        }

        case E_LOGIN_STATE_ENTER_GAME:
        {
            ptr = std::make_shared<EnterGameState>(emp);
            break;
        }
        case E_LGOIN_STATE_PLAYING:
        {
            ptr = std::make_shared<PlayingState>(emp);
            break;
        }
        case E_LOGIN_STATE_WAITING_ENTER_GAME:
        {
            ptr = std::make_shared<WaitingEnterGameState>(emp);
            break;
        }
        case E_LOGIN_STATE_NO_PLAYER:
        {
            ptr = std::make_shared<NoPlayerState>(emp);
            break;
        }
        case E_LOGIN_STATE_FULL_PLAYER:
        {
            ptr = std::make_shared<FullPlayerState>(emp);
            break;
        }
        default:
            break;
        }
        return ptr;
    }

}//namespace common

