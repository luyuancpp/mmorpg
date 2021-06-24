#include "login_player.h"

namespace common
{
    static int32_t kMaxPlayerSize = 4;

    void LoginPlayer::WaitingEnterGame()
    {

    }

    void LoginPlayer::FullPlayer()
    {

    }

    bool LoginPlayer::IsFullPlayer() const
    {
        return account_data_.simple_players().players_size() >= kMaxPlayerSize;
    }

}