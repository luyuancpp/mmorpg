#ifndef COMMON_SRC_LOGIN_STATE_LOGIN_STATE_H_
#define COMMON_SRC_LOGIN_STATE_LOGIN_STATE_H_

#include <memory>

#include "login_event.h"
#include "src/event/event.h"

namespace common
{
    //为什么要有状态，因为要处理可能我在登录的任意阶段发各种协议，比如加载数据过程中发一次进入游戏
    //这时候服务器数据流不能出错，应该告诉客户端请稍等，正在登录中
    //学楚留香手游，一个账号支持多个游戏角色在线
    enum EnumLoginState : uint8_t
    {
        E_LOGIN_NONE,
        E_LOGIN_ACCOUNT_LOGIN,//登录状态，重复登录的话提示，比如快点俩下
        E_LOGIN_ACCOUNT_LOADING,//登录状态，加载数据,防止客户端发其他协议提示，服务器数据流错乱安安
        E_LOGIN_ACCOUNT_CREATE_PLAYER,//登录状态，创建账号
        E_LOGIN_ACCOUNT_NORMAL,//登录状态，账号加载数据完成,可以进入游戏
        E_LOGIN_ACCOUNT_ENTER_GAME,//登录状态，有角色进入了游戏
        E_LGOIN_ACCOUNT_PLAYING,
        E_LOGIN_WAITING_ENTER_GAME,
        E_LOGIN_NO_PLAYER,
        E_LOGIN_ULL_PLAYER,
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
            emp_->emit(EeventLoginSetState{ E_LOGIN_WAITING_ENTER_GAME });
        }
        virtual void OnEmptyPlayer() {}
        void OnFullPlayer(){ emp_->emit(EeventLoginSetState{ E_LOGIN_ULL_PLAYER }); }
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

#endif//COMMON_SRC_LOGIN_STATE_LOGIN_STATE_H_
