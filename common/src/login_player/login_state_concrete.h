#ifndef COMMON_SRC_LOGIN_PLAYER_LOGIN_STATE_CONCRETE_H_
#define COMMON_SRC_LOGIN_PLAYER_LOGIN_STATE_CONCRETE_H_

//ensure you must not include this file 

#include "login_state.h"
#include "src/return_code/return_notice_code.h"

namespace common
{
    class NoneState : public ILoginState
    {
    public:
        using ILoginState::ILoginState;

        virtual uint32_t Login()override
        {
            emp_->emit(LoginESSetState{ E_LOGIN_STATE_LOGIN });
            return RET_OK;
        }

    };

    class LoginState : public ILoginState
    {
    public:
        using ILoginState::ILoginState;

        virtual void NoPlayer() override 
        {
            emp_->emit(LoginESSetState{ E_LOGIN_STATE_NO_PLAYER });
        }
    };

    class CreatePlayerState : public ILoginState
    {
    public:
        using ILoginState::ILoginState;
    };

    class EnterGameState : public ILoginState
    {
    public:
        using ILoginState::ILoginState;

        virtual void Playing()override { emp_->emit(LoginESSetState{ E_LGOIN_STATE_PLAYING }); }

    };

    class PlayingState : public ILoginState
    {
    public:
        using ILoginState::ILoginState;
    };

    class WaitingEnterGameState : public ILoginState
    {
    public:
        using ILoginState::ILoginState;
  
        virtual uint32_t CreatePlayer() override
        {
            emp_->emit(LoginESSetState{ E_LOGIN_STATE_CREATE_PLAYER });
            return RET_OK;
        }

        virtual uint32_t EnterGame()override
        {
            emp_->emit(LoginESSetState{ E_LOGIN_STATE_ENTER_GAME });
            return RET_OK;
        }
    };

    class NoPlayerState : public ILoginState
    {
    public:
        using ILoginState::ILoginState;

        virtual uint32_t CreatePlayer() override
        {
            emp_->emit(LoginESSetState{ E_LOGIN_STATE_CREATE_PLAYER });
            return RET_OK;
        }

        virtual uint32_t EnterGame()override
        {
            return RET_LOGIN_LOGIN_NO_PLAYER;
        }
    };

    class FullPlayerState : public ILoginState
    {
    public:
        using ILoginState::ILoginState;
        virtual uint32_t CreatePlayer() override
        {
            return RET_LOGIN_MAX_PLAYER_SIZE;
        }

        virtual uint32_t EnterGame()override
        {
            emp_->emit(LoginESSetState{ E_LOGIN_STATE_ENTER_GAME });
            return RET_OK;
        }
    };

}//namespace common

#endif//COMMON_SRC_LOGIN_PLAYER_LOGIN_STATE_CONCRETE_H_