#ifndef COMMON_SRC_LOGIN_PLAYER_LOGIN_STATE_H_
#define COMMON_SRC_LOGIN_PLAYER_LOGIN_STATE_H_

#include <memory>

#include "login_event.h"
#include "src/event/event.h"

namespace common
{
    enum EnumLoginState : uint8_t
    {
        E_LOGIN_STATE_NONE,
        E_LOGIN_STATE_LOGIN,
        E_LOGIN_STATE_CREATE_PLAYER,
        E_LOGIN_STATE_ENTER_GAME,
        E_LGOIN_STATE_PLAYING,
        E_LOGIN_STATE_MAX,
    };


    //login state interfase
    class LoginStateInterfase
    {
    public:
        using StatePtr = std::shared_ptr<LoginStateInterfase>;
        
        LoginStateInterfase(EventManagerPtr& emp)
            : emp_(emp)
        {

        }
        virtual ~LoginStateInterfase() {};

        virtual int32_t Login() = 0;
        virtual int32_t NoPlayer();
        virtual int32_t CreatePlayer() = 0;
        virtual int32_t CreatePlayerComplete() = 0;
        virtual int32_t EnterGame() = 0;
        virtual int32_t Playing() = 0;

        static StatePtr CreateState(int32_t state_enum, EventManagerPtr& emp);
    protected:
        EventManagerPtr emp_;
    };
}//namespace common

#endif//COMMON_SRC_LOGIN_PLAYER_LOGIN_STATE_H_
