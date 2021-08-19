#ifndef COMMON_SRC_LOGIN_STATE_LOGIN_STATE_CONCRETE_H_
#define COMMON_SRC_LOGIN_STATE_LOGIN_STATE_CONCRETE_H_

//ensure you must not include this file 

#include "login_state.h"
#include "src/return_code/error_code.h"

namespace common
{
    class NoneState : public LoginStateBase<NoneState, RET_LOGIN_DONOT_LOGIN>
    {
    public:
        using LoginStateBase::LoginStateBase;

        virtual uint32_t Login()override
        {
            emp_->emit(LoginESSetState{ E_LOGIN_STATE_LOGIN });
            return RET_OK;
        }

        virtual void WaitingEnterGame()override{}
    };

    class LoginState : public LoginStateBase<LoginState, RET_LOGIN_LOGIN_ING>
    {
    public:
        using LoginStateBase::LoginStateBase;

        virtual void OnEmptyPlayer() override 
        {
            emp_->emit(LoginESSetState{ E_LOGIN_STATE_NO_PLAYER });
        }
    };

    class CreatePlayerState : public LoginStateBase<CreatePlayerState, RET_LOGIN_BEING_CREATE_PLAYER>
    {
    public:
        using LoginStateBase::LoginStateBase;
    };

    class EnterGameState : public LoginStateBase< EnterGameState, RET_LOGIN_BEING_ENTER_GAME>
    {
    public:
        using LoginStateBase::LoginStateBase;
        virtual void OnPlaying()override { emp_->emit(LoginESSetState{ E_LGOIN_STATE_PLAYING }); }
    };

    class PlayingState : public LoginStateBase <PlayingState, RET_LOGIN_PLAYEING>
    {
    public:
        using LoginStateBase::LoginStateBase;
    };

    class WaitingEnterGameState : public LoginStateBase<WaitingEnterGameState, RET_LOGIN_WAITING_ENTER_GAME>
    {
    public:
        using LoginStateBase::LoginStateBase;
  
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

    class EmptyPlayerState : public LoginStateBase <EmptyPlayerState, RET_LOGIN_WAITING_ENTER_GAME>
    {
    public:
        using LoginStateBase::LoginStateBase;

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

    class FullPlayerState : public LoginStateBase <FullPlayerState, RET_LOGIN_WAITING_ENTER_GAME>
    {
    public:
        using LoginStateBase::LoginStateBase;
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

#endif//COMMON_SRC_LOGIN_STATE_LOGIN_STATE_CONCRETE_H_