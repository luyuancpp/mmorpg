#pragma once

#include <array>
#include <cstdint>

#include "login_state.h"

class LoginStateMachine
{
public:
	using StatePtr = std::shared_ptr<IAccountState>;
	using StatePtrList = std::array<StatePtr, kLoginStateMax>;

	LoginStateMachine();

	void set_state(uint32_t state_enum);

	uint32_t Login();
	uint32_t Logout();
	uint32_t CreatePlayer();
	uint32_t EnterGame();

	void WaitingEnterGame();
	void OnEmptyPlayer();
	void OnFullPlayer();
	void OnPlaying();

private:
	StatePtrList state_list_;
	StatePtr current_state_;
};
