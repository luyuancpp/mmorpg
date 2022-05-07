#pragma once

#include "src/common_type/common_type.h"
#include "src/game_logic/login_state/login_state_machine.h"
#include "src/game_logic/game_registry.h"

#include "mysql_database_table.pb.h"

class AccountPlayer
{
public:
    void set_account_data(const account_database& pb) { account_data_ = pb; }
    account_database& account_data(){ return account_data_; }
    const std::string& account() const{ return account_data_.account(); }
    Guid PlayingId() const { return playing_id_; }

    bool IsInPlayerList(Guid guid);

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

private:
    bool IsFullPlayer()const;
    bool EmptyPlayer()const;

    LoginStateMachine login_state_machine_;
    ::account_database account_data_;
    Guid playing_id_{ kInvalidGuid };
};


struct MSLoginAccount : public EntityPtr
{
    GuidVector playing_guids_;
};
struct AccountLoginNode
{
    uint32_t login_node_id_{ UINT32_MAX };
    uint32_t gate_node_id_{ UINT32_MAX };
};

