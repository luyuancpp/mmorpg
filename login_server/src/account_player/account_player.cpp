#include "account_player.h"

namespace gw2l
{
    static int32_t kMaxPlayerSize = 4;

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