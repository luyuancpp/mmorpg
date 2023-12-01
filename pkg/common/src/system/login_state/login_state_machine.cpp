#include "login_state_machine.h"

#include "src/game_logic/tips_id.h"
#include "login_state_concrete.h"

LoginStateMachine::LoginStateMachine()
{
    set_state(kLoginNone);
}

void LoginStateMachine::set_state(uint32_t state_enum)
{
    switch (state_enum)
    {
    case kLoginNone:
    {
        current_state_ = std::make_unique<NoneState>(*this);
    }
        break;
	case kLoginAccountIngBeingProcessing:
	{
        current_state_ = std::make_unique<LoginState>(*this);
	}
	    break;
	case kLoginAccountCreatePlayer:
	{
		current_state_ = std::make_unique<CreatePlayerState>(*this);
	}
	    break;
	case kLoginAccountEnterGame:
	{
		current_state_ = std::make_unique<EnterGameState>(*this);
	}
	break;
	case kLoginAccountPlaying:
	{
		current_state_ = std::make_unique<PlayingState>(*this);
	}
	break;
	case kLoginWaitingEnterGame:
	{
		current_state_ = std::make_unique<WaitingEnterGameState>(*this);
	}
	break;
	case kLoginAccountNoPlayer:
	{
		current_state_ = std::make_unique<EmptyPlayerState>(*this);
	}
	break;
	case kLoginAccountFullPlayer:
	{
		current_state_ = std::make_unique<FullPlayerState>(*this);
	}
	break;
    default:
        break;
    }
}

uint32_t LoginStateMachine::Login()
{
    return current_state_->LoginAccount();
}

uint32_t LoginStateMachine::CreatePlayer()
{
    return current_state_->CreatePlayer();
}

uint32_t LoginStateMachine::EnterGame()
{
    return current_state_->EnterGame();
}

void LoginStateMachine::WaitingEnterGame()
{
    current_state_->WaitingEnterGame();
}

void LoginStateMachine::OnEmptyPlayer()
{
    current_state_->OnEmptyPlayer();
}

void LoginStateMachine::OnFullPlayer()
{
    current_state_->OnFullPlayer();
}

void LoginStateMachine::OnPlaying()
{
    current_state_->OnPlaying();
}
