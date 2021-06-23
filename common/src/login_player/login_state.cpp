#include "login_state.h"

#include "src/return_code/return_notice_code.h"


namespace common
{
    class NoneState : public LoginStateInterfase
    {
    public:
        using LoginStateInterfase::LoginStateInterfase;

        virtual int32_t Login()override
        {
            emp_->emit(LoginESSetState{ E_LOGIN_STATE_LOGIN });
            return RET_OK;
        }
        virtual int32_t CreatePlayer()override
        {
            return RET_LOGIN_LOGIN_NOT_COMPLETE;
        }
        virtual int32_t CreatePlayerComplete()override
        {
            return RET_LOGIN_LOGIN_NOT_COMPLETE;
        }
        virtual int32_t EnterGame()override
        {
            return RET_LOGIN_LOGIN_NOT_COMPLETE;
        }

        virtual int32_t Playing()override
        {
            return RET_LOGIN_LOGIN_NOT_COMPLETE;
        }
    };

    class LoginState : public LoginStateInterfase
    {
    public:
        using LoginStateInterfase::LoginStateInterfase;
        virtual int32_t Login()override
        {
            return RET_LOGIN_REPETITION_LOGIN;
        }
        virtual int32_t CreatePlayer()override
        {
            emp_->emit(LoginESSetState{ E_LOGIN_STATE_CREATE_PLAYER });
            return RET_OK;
        }
        virtual int32_t CreatePlayerComplete()override
        {
            return RET_OK;
        }
        virtual int32_t EnterGame()override
        {
            return RET_LOGIN_LOGIN_NO_PLAYER;
        }

        virtual int32_t Playing()override
        {
            return RET_OK;
        }
    };


    class CreatePlayerState : public LoginStateInterfase
    {
    public:
        using LoginStateInterfase::LoginStateInterfase;
        virtual int32_t Login()override
        {
            return RET_OK;
        }
        virtual int32_t CreatePlayer()override
        {
            return RET_OK;
        }
        virtual int32_t CreatePlayerComplete()override
        {
            return RET_OK;
        }
        virtual int32_t EnterGame()override
        {
            return RET_OK;
        }

        virtual int32_t Playing()override
        {
            return RET_OK;
        }
    };

    class EnterGameState : public LoginStateInterfase
    {
    public:
        using LoginStateInterfase::LoginStateInterfase;
        virtual int32_t Login()override
        {
            return RET_OK;
        }
        virtual int32_t CreatePlayer()override
        {
            return RET_OK;
        }
        virtual int32_t CreatePlayerComplete()override
        {
            return RET_OK;
        }
        virtual int32_t EnterGame()override
        {
            return RET_OK;
        }

        virtual int32_t Playing()override
        {
            return RET_OK;
        }
    };


    class PlayingState : public LoginStateInterfase
    {
    public:
        using LoginStateInterfase::LoginStateInterfase;
        virtual int32_t Login()override
        {
            return RET_OK;
        }
        virtual int32_t CreatePlayer()override
        {
            return RET_OK;
        }
        virtual int32_t CreatePlayerComplete()override
        {
            return RET_OK;
        }
        virtual int32_t EnterGame()override
        {
            return RET_OK;
        }

        virtual int32_t Playing()override
        {
            return RET_OK;
        }
    };

    int32_t LoginStateInterfase::NoPlayer()
    {
        return RET_OK;
    }

    LoginStateInterfase::StatePtr LoginStateInterfase::CreateState(int32_t state_enum, EventManagerPtr& emp)
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
        default:
            break;
        }
        return ptr;
    }

}//namespace common

