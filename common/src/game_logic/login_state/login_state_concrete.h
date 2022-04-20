#ifndef COMMON_SRC_LOGIN_STATE_LOGIN_STATE_CONCRETE_H_
#define COMMON_SRC_LOGIN_STATE_LOGIN_STATE_CONCRETE_H_

//ensure you must not include this file 

#include "login_state.h"
#include "src/return_code/error_code.h"

namespace common
{
    class NoneState : public LoginStateBase<RET_LOGIN_DONOT_LOGIN>
    {
    public:
        using LoginStateBase::LoginStateBase;

        virtual uint32_t LoginAccount()override
        {
            login_machine_.set_state(kLoginAccountLogining);
            return kRetOK;
        }

        virtual void WaitingEnterGame()override{}
    };

    class LoginState : public LoginStateBase<RET_LOGIN_LOGIN_ING>
    {
    public:
        using LoginStateBase::LoginStateBase;

        virtual void OnEmptyPlayer() override 
        {
            login_machine_.set_state(kLoginAccountNoPlayer );
        }
    };

    class CreatePlayerState : public LoginStateBase<RET_LOGIN_BEING_CREATE_PLAYER>
    {
    public:
        using LoginStateBase::LoginStateBase;
    };

    class EnterGameState : public LoginStateBase<RET_LOGIN_BEING_ENTER_GAME>
    {
    public:
        using LoginStateBase::LoginStateBase;
        virtual void OnPlaying()override { login_machine_.set_state(kLoignAccountPling ); }
    };

    class PlayingState : public LoginStateBase <RET_LOGIN_PLAYEING>
    {
    public:
        using LoginStateBase::LoginStateBase;
    };

    class WaitingEnterGameState : public LoginStateBase<RET_LOGIN_WAITING_ENTER_GAME>
    {
    public:
        using LoginStateBase::LoginStateBase;
  
        virtual uint32_t CreatePlayer() override
        {
            login_machine_.set_state(kLoginAcccountCreatePlayer );
            return kRetOK;
        }

        virtual uint32_t EnterGame()override
        {
            login_machine_.set_state(kLoginAccountEnterGame );
            return kRetOK;
        }
    };

    class EmptyPlayerState : public LoginStateBase <RET_LOGIN_WAITING_ENTER_GAME>
    {
    public:
        using LoginStateBase::LoginStateBase;

        virtual uint32_t CreatePlayer() override
        {
            login_machine_.set_state(kLoginAcccountCreatePlayer);
            return kRetOK;
        }

        virtual uint32_t EnterGame()override
        {
            return RET_LOGIN_LOGIN_NO_PLAYER;
        }
    };

    class FullPlayerState : public LoginStateBase < RET_LOGIN_WAITING_ENTER_GAME>
    {
    public:
        using LoginStateBase::LoginStateBase;
        virtual uint32_t CreatePlayer() override
        {
            return RET_LOGIN_MAX_PLAYER_SIZE;
        }

        virtual uint32_t EnterGame()override
        {
            login_machine_.set_state(kLoginAccountEnterGame);
            return kRetOK;
        }
    };

}//namespace common

#endif//COMMON_SRC_LOGIN_STATE_LOGIN_STATE_CONCRETE_H_