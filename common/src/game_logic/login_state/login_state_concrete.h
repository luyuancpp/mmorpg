#pragma once

//ensure you must not include this file 

#include "login_state.h"
#include "src/return_code/error_code.h"


class NoneState : public LoginStateBase<kRetLoginHadnotLogin>
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

class LoginState : public LoginStateBase<kRetLoginIng>
{
public:
    using LoginStateBase::LoginStateBase;

    virtual void OnEmptyPlayer() override 
    {
        login_machine_.set_state(kLoginAccountNoPlayer );
    }
};

class CreatePlayerState : public LoginStateBase<kRetLoignCreatingPlayer>
{
public:
    using LoginStateBase::LoginStateBase;
};

class EnterGameState : public LoginStateBase<kRetLoginEnteringGame>
{
public:
    using LoginStateBase::LoginStateBase;
    virtual void OnPlaying()override { login_machine_.set_state(kLoignAccountPling ); }
};

class PlayingState : public LoginStateBase <kRetLoginPlaying>
{
public:
    using LoginStateBase::LoginStateBase;
};

class WaitingEnterGameState : public LoginStateBase<kRetLoignWatingEnterGame>
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

class EmptyPlayerState : public LoginStateBase <kRetLoignWatingEnterGame>
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
        return kRetLoginPlayerGuidError;
    }
};

class FullPlayerState : public LoginStateBase < kRetLoignWatingEnterGame>
{
public:
    using LoginStateBase::LoginStateBase;
    virtual uint32_t CreatePlayer() override
    {
        return kRetLoginAccountPlayerFull;
    }

    virtual uint32_t EnterGame()override
    {
        login_machine_.set_state(kLoginAccountEnterGame);
        return kRetOK;
    }
};
