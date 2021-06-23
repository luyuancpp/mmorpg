#ifndef COMMON_SRC_LOGIN_PLAYER_LOGIN_STATE_MACHINE_H_
#define COMMON_SRC_LOGIN_PLAYER_LOGIN_STATE_MACHINE_H_

#include <array>
#include <cstdint>

#include "login_state.h"

namespace common
{
    class LoginStateMachine :  public Receiver<LoginStateMachine>
    {
    public:

        using StatePtr = std::shared_ptr<LoginStateInterfase>;
        using StatePtrList = std::array<StatePtr, E_LOGIN_STATE_MAX>;

        LoginStateMachine();

        void set_state(int32_t state_enum);

        int32_t Lgoin();
        int32_t NoPlayer();
        int32_t CreatePlayer();
        int32_t CreatePlayerComplete();
        int32_t EnterGame();
        int32_t Playing();

        void receive(const LoginESSetState& s);
    private:
        StatePtrList state_list_;
        StatePtr current_state_;
        EventManagerPtr emp_;
    };
}//namespace common

#endif//COMMON_SRC_LOGIN_PLAYER_LOGIN_PLAYER_H_
