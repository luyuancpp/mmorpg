#pragma once

#include <memory>


//为什么要有状态，因为要处理可能我在登录的任意阶段发各种协议，比如加载数据过程中发一次进入游戏
//这时候服务器数据流不能出错，应该告诉客户端请稍等，正在登录中
//学楚留香手游，一个账号支持多个游戏角色在线
enum EnumLoginState : uint8_t
{
    kLoginNone,
    kLoginAccountLogining,//登录状态，重复登录的话提示，比如快点俩下
    kLoginAcccountCreatePlayer,//登录状态，创建账号
    kLoginAccountEnterGame,//登录状态，有角色进入了游戏
    kLoignAccountPling,
    kLoginWatingEnterGame,//登录状态，账号加载数据完成,可以进入游戏
    kLoginAccountNoPlayer,//登录状态，没有角色，不能进入游戏，必须发创建角色协议
    kLoginAccountFullPlayer,//登录状态，角色已经满了不能再创建角色了
    kLoginStateMax,
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





