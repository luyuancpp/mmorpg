#ifndef COMMON_SRC_LOGIN_STATE_LOGIN_STATE_H_
#define COMMON_SRC_LOGIN_STATE_LOGIN_STATE_H_

#include <memory>

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
        E_LOGIN_ACCOUNT_ENTER_GAME,//登录状态，有角色进入了游戏
        E_LGOIN_ACCOUNT_PLAYING,
        E_LOGIN_WAITING_ENTER_GAME,//登录状态，账号加载数据完成,可以进入游戏
        E_LOGIN_ACCOUNT_NO_PLAYER,//登录状态，没有角色，不能进入游戏，必须发创建角色协议
        E_LOGIN_ACCOUNT_FULL_PLAYER,//登录状态，角色已经满了不能再创建角色了
        E_LOGIN_STATE_MAX,
    };

    class LoginStateMachine;

    //login state interfase
    class IAccountState 
    {
    public:
        using StatePtr = std::shared_ptr<IAccountState>;

        IAccountState(LoginStateMachine& m)
            : login_machine_(m)
        {}
        virtual ~IAccountState() {};

        // player operator
        virtual uint32_t LoginAccount() = 0;
        uint32_t LogoutAccount();
        virtual uint32_t CreatePlayer() = 0;
        virtual uint32_t EnterGame() = 0;
        // server operator
        virtual void WaitingEnterGame();

        virtual void OnEmptyPlayer() {}
        void OnFullPlayer();
        virtual void OnPlaying() {}
 
    protected:
        LoginStateMachine& login_machine_;
    };

    //login state interfase
    template <typename Derived, uint32_t ProcessingCode>
    class LoginStateBase : public IAccountState
    {
    public:
        using IAccountState::IAccountState;

        uint32_t processing()const { return processing_code_; }

        virtual uint32_t LoginAccount() override { return processing(); }
        virtual uint32_t CreatePlayer() override { return processing(); }
        virtual uint32_t EnterGame()override { return processing(); }

    protected:
        static const uint32_t processing_code_{ ProcessingCode };
    };
}//namespace common

#endif//COMMON_SRC_LOGIN_STATE_LOGIN_STATE_H_
