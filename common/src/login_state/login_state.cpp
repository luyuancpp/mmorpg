#include "login_state.h"

#include "login_state_concrete.h"

namespace common
{

    uint32_t ILoginState::Logout()
    {
        emp_->emit(LoginESSetState{ E_LOGIN_NONE });
        return RET_OK;
    }

    ILoginState::StatePtr ILoginState::CreateState(int32_t state_enum, EventManagerPtr& emp)
    {
        StatePtr ptr;
        switch (state_enum)
        {
        case E_LOGIN_NONE:
        {
            CreateILoginStateP cp{ emp };
            ptr = std::make_shared<NoneState>(cp);
            break;
        }
        case E_LOGIN_LOGIN:
        {
            CreateILoginStateP cp{ emp };
            ptr = std::make_shared<LoginState>(cp);
            break;
        }
        case E_LOGIN_CREATE_PLAYER:
        {
            CreateILoginStateP cp{ emp };
            ptr = std::make_shared<CreatePlayerState>(cp);
            break;
        }
        case E_LOGIN_ENTER_GAME:
        {
            CreateILoginStateP cp{ emp };
            ptr = std::make_shared<EnterGameState>(cp);
            break;
        }
        case E_LGOIN_PLAYING:
        {
            CreateILoginStateP cp{ emp };
            ptr = std::make_shared<PlayingState>(cp);
            break;
        }
        case E_LOGIN_WAITING_ENTER_GAME:
        {
            CreateILoginStateP cp{emp};
            ptr = std::make_shared<WaitingEnterGameState>(cp);
            break;
        }
        case E_LOGIN_NO_PLAYER:
        {
            CreateILoginStateP cp{ emp };
            ptr = std::make_shared<EmptyPlayerState>(cp);
            break;
        }
        case E_LOGIN_ULL_PLAYER:
        {
            CreateILoginStateP cp{ emp };
            ptr = std::make_shared<FullPlayerState>(cp);
            break;
        }
        default:
            break;
        }
        return ptr;
    }

}//namespace common

