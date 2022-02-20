#ifndef MASTER_SERVER_SRC_LOGIN_SERVICE_H_
#define MASTER_SERVER_SRC_LOGIN_SERVICE_H_

#include "l2ms.pb.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/account_player/ms_account.h"
#include "src/common_type/common_type.h"
#include "src/comp/ms_login_account_comp.hpp"
#include "src/game_logic/comp/account_comp.hpp"
///<<< END WRITING YOUR CODE
namespace l2ms
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
    class LoginServiceImpl : public l2ms::LoginService
    {
    public:
      
        void LoginAccount(::google::protobuf::RpcController* controller,
            const ::l2ms::LoginAccountRequest* request,
            ::l2ms::LoginAccountResponse* response,
            ::google::protobuf::Closure* done)override;

        void EnterGame(::google::protobuf::RpcController* controller,
            const ::l2ms::EnterGameRequest* request,
            ::l2ms::EnterGameResponese* response,
            ::google::protobuf::Closure* done)override;

        void LeaveGame(::google::protobuf::RpcController* controller,
            const ::l2ms::LeaveGameRequest* request,
            ::google::protobuf::Empty* response,
            ::google::protobuf::Closure* done)override;

        void Disconect(::google::protobuf::RpcController* controller,
            const ::l2ms::DisconnectRequest* request,
            ::google::protobuf::Empty* response,
            ::google::protobuf::Closure* done)override;

///<<< BEGIN WRITING YOUR CODE
		using AccountMap = std::unordered_map<std::string, master::MSLoginAccount>;

    private:
        AccountMap logined_accounts_;
///<<< END WRITING YOUR CODE
    };
}

#endif//MASTER_SERVER_SRC_LOGIN_SERVICE_H_
