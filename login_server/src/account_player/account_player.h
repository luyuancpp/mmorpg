#ifndef LOGIN_SRC_ACCOUNT_PLAYER_ACCOUNT_PLAYER_H_
#define LOGIN_SRC_ACCOUNT_PLAYER_ACCOUNT_PLAYER_H_

#include "src/common_type/common_type.h"
#include "src/login_player/login_state_machine.h"
#include "mysql_database_table.pb.h"

namespace gw2l
{
    class AccountPlayer
    {
    public:
        void set_account_data(const account_database& pb) { account_data_ = pb; }
        account_database& account_data(){ return account_data_; }
        const std::string& account() const{ return account_data_.account(); }

        bool IsPlayerId(common::GameGuid player_id);

        // login state machine
        inline uint32_t Login() { return login_state_machine_.Login(); }
        inline uint32_t Logout() { return login_state_machine_.Logout(); }
        inline uint32_t CreatePlayer() { return login_state_machine_.CreatePlayer(); }
        inline uint32_t EnterGame() { return login_state_machine_.EnterGame(); }

        void OnDbLoaded();
        inline void Playing() { return login_state_machine_.OnPlaying(); }

    private:
        bool IsFullPlayer()const;
        bool EmptyPlayer()const;

        common::LoginStateMachine login_state_machine_;
        ::account_database account_data_;
    };
}//namespace gw2l

#endif//LOGIN_SRC_ACCOUNT_PLAYER_ACCOUNT_PLAYER_H_
