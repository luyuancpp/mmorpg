#ifndef COMMON_SRC_LOGIN_STATE_LOGIN_STATE_H_
#define COMMON_SRC_LOGIN_STATE_LOGIN_STATE_H_

#include <memory>

namespace common
{
    //ΪʲôҪ��״̬����ΪҪ����������ڵ�¼������׶η�����Э�飬����������ݹ����з�һ�ν�����Ϸ
    //��ʱ����������������ܳ���Ӧ�ø��߿ͻ������Եȣ����ڵ�¼��
    //ѧ���������Σ�һ���˺�֧�ֶ����Ϸ��ɫ����
    enum EnumLoginState : uint8_t
    {
        E_LOGIN_NONE,
        E_LOGIN_ACCOUNT_LOGIN,//��¼״̬���ظ���¼�Ļ���ʾ������������
        E_LOGIN_ACCOUNT_LOADING,//��¼״̬����������,��ֹ�ͻ��˷�����Э����ʾ�����������������Ұ���
        E_LOGIN_ACCOUNT_CREATE_PLAYER,//��¼״̬�������˺�
        E_LOGIN_ACCOUNT_ENTER_GAME,//��¼״̬���н�ɫ��������Ϸ
        E_LGOIN_ACCOUNT_PLAYING,
        E_LOGIN_WAITING_ENTER_GAME,//��¼״̬���˺ż����������,���Խ�����Ϸ
        E_LOGIN_ACCOUNT_NO_PLAYER,//��¼״̬��û�н�ɫ�����ܽ�����Ϸ�����뷢������ɫЭ��
        E_LOGIN_ACCOUNT_FULL_PLAYER,//��¼״̬����ɫ�Ѿ����˲����ٴ�����ɫ��
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
