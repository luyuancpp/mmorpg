#ifndef COMMON_SRC_LOGIN_PLAYER_LOGIN_PLAYER_H_
#define COMMON_SRC_LOGIN_PLAYER_LOGIN_PLAYER_H_

#include "login_state_machine.h"
#include "mysql_database_table.pb.h"

namespace common
{
    class LoginPlayer
    {
    public:
        void set_account_data(const account_database& pb) { account_data_ = pb; }
        account_database& account_data(){ return account_data_; }
        const std::string& account() const{ return account_data_.account(); }

        // login state machine
        inline uint32_t Login() { return login_state_machine_.Login(); }
        inline uint32_t Logout() { return login_state_machine_.Logout(); }
        inline uint32_t CreatePlayer() { return login_state_machine_.CreatePlayer(); }
        inline uint32_t EnterGame() { return login_state_machine_.EnterGame(); }

        void WaitingEnterGame();
        inline void NoPlayer() { login_state_machine_.NoPlayer(); }
        void FullPlayer();
        inline void Playing() { return login_state_machine_.Playing(); }

        bool IsFullPlayer()const;

    private:
        LoginStateMachine login_state_machine_;
        ::account_database account_data_;
    };
}//namespace common

#endif//COMMON_SRC_LOGIN_PLAYER_LOGIN_PLAYER_H_
