#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include "login_state.h"

class LoginStateMachine
{
public:
	using StatePtr = std::unique_ptr<IAccountState>;

	LoginStateMachine();

	void set_state(uint32_t state_enum);

	uint32_t Login();;
	uint32_t CreatePlayer();
	uint32_t EnterGame();

	void WaitingEnterGame();
	void OnEmptyPlayer();
	void OnFullPlayer();
	void OnPlaying();

private:
	StatePtr current_state_;
};
