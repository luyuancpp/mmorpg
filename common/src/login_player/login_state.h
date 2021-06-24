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
        E_LOGIN_STATE_WAITING_ENTER_GAME,
        E_LOGIN_STATE_NO_PLAYER,
        E_LOGIN_STATE_FULL_PLAYER,
        E_LOGIN_STATE_MAX,
    };

    struct CreateILoginStateP
    {
        CreateILoginStateP(EventManagerPtr& emp, uint32_t processing_code)
            : emp_(emp),
            processing_code_(processing_code)
        {

        }
        EventManagerPtr& emp_;
        uint32_t processing_code_;
    };

    //login state interfase
    class ILoginState
    {
    public:
        using StatePtr = std::shared_ptr<ILoginState>;

        ILoginState(CreateILoginStateP& c)
            : emp_(c.emp_),
              processing_code_(c.processing_code_)
        {

        }
        virtual ~ILoginState() {};

        void set_processing(uint32_t p) { processing_code_ = p; }
        uint32_t processing()const { return processing_code_; }

        virtual uint32_t Login() { return processing(); }
        uint32_t Logout();
        virtual uint32_t CreatePlayer() { return processing(); }
        virtual uint32_t EnterGame() { return processing(); }

        virtual void WaitingEnterGame()
        {
            emp_->emit(LoginESSetState{ E_LOGIN_STATE_WAITING_ENTER_GAME });
        }
        virtual void NoPlayer() {}
        virtual void FullPlayer(){ emp_->emit(LoginESSetState{ E_LOGIN_STATE_FULL_PLAYER }); }
        virtual void Playing() {}
  
        static StatePtr CreateState(int32_t state_enum, EventManagerPtr& emp);
    protected:
        EventManagerPtr emp_;
        uint32_t processing_code_{0};
    };
}//namespace common

#endif//COMMON_SRC_LOGIN_PLAYER_LOGIN_STATE_H_
