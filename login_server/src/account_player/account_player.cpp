#include "account_player.h"

namespace gw2l
{
    static int32_t kMaxPlayerSize = 4;

    bool AccountPlayer::IsPlayerId(common::GameGuid guid)
    {
        auto s = account_data_.simple_players().players_size();
        for (int32_t i = 0; i < s; ++i)
        {
            if (account_data_.simple_players().players(i).guid() == guid)
            {
                return true;
            }            
        }
        return false;
    }

    void AccountPlayer::OnDbLoaded()
    {
        if (IsFullPlayer())
        {
            login_state_machine_.OnFullPlayer();
        }
        else if (EmptyPlayer())
        {
            login_state_machine_.OnEmptyPlayer();
        }
        else
        {
            login_state_machine_.WaitingEnterGame();
        }
    }

    bool AccountPlayer::IsFullPlayer() const
    {
        return account_data_.simple_players().players_size() >= kMaxPlayerSize;
    }

    bool AccountPlayer::EmptyPlayer() const
    {
        return account_data_.simple_players().players_size() <= 0;
    }

}