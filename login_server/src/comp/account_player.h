#pragma once

#include "mysql_database_table.pb.h"
#include "src/common_type/common_type.h"
#include "src/game_logic/login_state/login_state_machine.h"

class AccountPlayer
{
public:
    void set_account_data(const account_database& account_pb) { account_data_ = account_pb; }
    account_database& account_data() { return account_data_; }
    const std::string& account() const { return account_data_.account(); }
    Guid PlayingId() const { return playing_id_; }

    void set_login_account_name(const std::string& name) { login_account_name_ = name; }
    const std::string& login_account_name() const { return login_account_name_; }

    bool HasPlayer(Guid guid);

    // login state machine
    inline uint32_t Login() { return login_state_machine_.Login(); }
    inline uint32_t CreatePlayer() { return login_state_machine_.CreatePlayer(); }
    inline uint32_t EnterGame() { return login_state_machine_.EnterGame(); }

    void OnDbLoaded();

    inline void Playing(Guid playing_id)
    {
        playing_id_ = playing_id;
        login_state_machine_.OnPlaying();
    }

    bool IsValid() const { return account_data_.ByteSizeLong() > 0; }
private:
    bool IsFullPlayer() const;
    bool EmptyPlayer() const;

    LoginStateMachine login_state_machine_;
    ::account_database account_data_;
    Guid playing_id_{kInvalidGuid};
    std::string login_account_name_;
};

