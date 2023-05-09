#pragma once

//ensure you must not include this file 

#include "login_state.h"
#include "src/game_logic/tips_id.h"


class NoneState : public IAccountState
{
public:
    using IAccountState::IAccountState;

    virtual uint32_t LoginAccount()override
    {
        login_machine_.set_state(kLoginAccountIngBeingProcessing);
        return kRetOK;
    }
    virtual uint32_t CreatePlayer() override { return kRetLoginHadnotLogin; }
    virtual uint32_t EnterGame()override { return kRetLoginHadnotLogin; }


    virtual void WaitingEnterGame()override{}
};

class LoginState : public IAccountState
{
public:
    using IAccountState::IAccountState;

    virtual uint32_t LoginAccount() override { return kRetLoginIng; }
    virtual uint32_t CreatePlayer() override { return kRetLoginIng; }
    virtual uint32_t EnterGame()override { return kRetLoginIng; }

    virtual void OnEmptyPlayer() override 
    {
        login_machine_.set_state(kLoginAccountNoPlayer );
    }
};

class CreatePlayerState : public IAccountState
{
public:
    using IAccountState::IAccountState;
    virtual uint32_t LoginAccount() override { return kRetLoignCreatingPlayer; }
    virtual uint32_t CreatePlayer() override { return kRetLoignCreatingPlayer; }
    virtual uint32_t EnterGame()override { return kRetLoignCreatingPlayer; }
};

class EnterGameState : public IAccountState
{
public:
    using IAccountState::IAccountState;
    virtual uint32_t LoginAccount() override { return kRetLoginEnteringGame; }
    virtual uint32_t CreatePlayer() override { return kRetLoginEnteringGame; }
    virtual uint32_t EnterGame()override { return kRetLoginEnteringGame; }
    virtual void OnPlaying()override { login_machine_.set_state(kLoginAccountPlaying ); }
};

class PlayingState : public IAccountState
{
public:
    using IAccountState::IAccountState;
    virtual uint32_t LoginAccount() override { return kRetLoginPlaying; }
    virtual uint32_t CreatePlayer() override { return kRetLoginPlaying; }
    virtual uint32_t EnterGame()override { return kRetLoginPlaying; }
};

class WaitingEnterGameState : public IAccountState
{
public:
    using IAccountState::IAccountState;
  
    virtual uint32_t LoginAccount() override { return kRetLoignWatingEnterGame; }
    virtual uint32_t CreatePlayer() override
    {
        login_machine_.set_state(kLoginAccountCreatePlayer);
        return kRetOK;
    }
    virtual uint32_t EnterGame()override
    {
        login_machine_.set_state(kLoginAccountEnterGame );
        return kRetOK;
    }
};

class EmptyPlayerState : public IAccountState
{
public:
    using IAccountState::IAccountState;

    virtual uint32_t LoginAccount() override { return kRetLoignWatingEnterGame; }
    virtual uint32_t CreatePlayer() override
    {
        login_machine_.set_state(kLoginAccountCreatePlayer);
        return kRetOK;
    }

    virtual uint32_t EnterGame()override
    {
        return kRetLoginPlayerGuidError;
    }
};

class FullPlayerState : public IAccountState
{
public:
    using IAccountState::IAccountState;

    virtual uint32_t LoginAccount() override { return kRetLoignWatingEnterGame; }
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
