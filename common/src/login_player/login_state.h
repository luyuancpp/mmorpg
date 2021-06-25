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
        CreateILoginStateP(EventManagerPtr& emp) : emp_(emp){}
        EventManagerPtr& emp_;
    };

    //login state interfase
    class ILoginState 
    {
    public:
        using StatePtr = std::shared_ptr<ILoginState>;

        ILoginState(CreateILoginStateP& c)
            : emp_(c.emp_)
        {

        }
        virtual ~ILoginState() {};

        // player operator
        virtual uint32_t Login() = 0;
        uint32_t Logout();
        virtual uint32_t CreatePlayer() = 0;
        virtual uint32_t EnterGame() = 0;

        // server operator
        virtual void WaitingEnterGame()
        {
            emp_->emit(LoginESSetState{ E_LOGIN_STATE_WAITING_ENTER_GAME });
        }
        virtual void OnEmptyPlayer() {}
        void OnFullPlayer(){ emp_->emit(LoginESSetState{ E_LOGIN_STATE_FULL_PLAYER }); }
        virtual void OnPlaying() {}
  
        static StatePtr CreateState(int32_t state_enum, EventManagerPtr& emp);

    protected:
        EventManagerPtr emp_;
    };

    //login state interfase
    template <typename Derived, uint32_t ProcessingCode>
    class LoginStateBase : public ILoginState
    {
    public:
        using ILoginState::ILoginState;

        uint32_t processing()const { return processing_code_; }

        virtual uint32_t Login() override { return processing(); }
        virtual uint32_t CreatePlayer() override { return processing(); }
        virtual uint32_t EnterGame()override { return processing(); }

    protected:
        static const uint32_t processing_code_{ ProcessingCode };
    };
}//namespace common

#endif//COMMON_SRC_LOGIN_PLAYER_LOGIN_STATE_H_
