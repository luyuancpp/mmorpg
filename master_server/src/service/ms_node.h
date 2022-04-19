#ifndef MASTER_SERVER_SRC_SERVICE_MS_NODE_H_
#define MASTER_SERVER_SRC_SERVICE_MS_NODE_H_
#include "ms_node.pb.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/account_player/ms_account.h"
#include "src/common_type/common_type.h"
#include "src/comp/ms_login_account_comp.h"
#include "src/game_logic/comp/account_comp.h"
#include "src/network/rpc_closure.h"

#include "gs_node.pb.h"
///<<< END WRITING YOUR CODE
namespace msservice{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class MasterNodeServiceImpl : public MasterNodeService{
public:
///<<< BEGIN WRITING YOUR CODE
	using AccountMap = std::unordered_map<std::string, MSLoginAccount>;

    using Ms2gsEnterGameRpcRplied = common::NormalClosure<gsservice::EnterGameRequest, gsservice::EnterGameRespone>;
	void Ms2gsEnterGameReplied(Ms2gsEnterGameRpcRplied replied);
private:
	AccountMap logined_accounts_;
///<<< END WRITING YOUR CODE
public:
    void StartGS(::google::protobuf::RpcController* controller,
        const msservice::StartGSRequest* request,
        msservice::StartGSResponse* response,
        ::google::protobuf::Closure* done)override;

    void OnGwConnect(::google::protobuf::RpcController* controller,
        const msservice::ConnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void OnGwLeaveGame(::google::protobuf::RpcController* controller,
        const msservice::LeaveGameRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void OnGwPlayerService(::google::protobuf::RpcController* controller,
        const msservice::ClientMessageRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void OnGwDisconnect(::google::protobuf::RpcController* controller,
        const msservice::DisconnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void OnLsLoginAccount(::google::protobuf::RpcController* controller,
        const msservice::LoginAccountRequest* request,
        msservice::LoginAccountResponse* response,
        ::google::protobuf::Closure* done)override;

    void OnLsEnterGame(::google::protobuf::RpcController* controller,
        const msservice::EnterGameRequest* request,
        msservice::EnterGameResponese* response,
        ::google::protobuf::Closure* done)override;

    void OnLsLeaveGame(::google::protobuf::RpcController* controller,
        const msservice::LsLeaveGameRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void OnLsDisconnect(::google::protobuf::RpcController* controller,
        const msservice::LsDisconnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void OGsPlayerService(::google::protobuf::RpcController* controller,
        const msservice::PlayerNodeServiceRequest* request,
        msservice::PlayerMessageRespone* response,
        ::google::protobuf::Closure* done)override;

};
}// namespace msservice
#endif//MASTER_SERVER_SRC_SERVICE_MS_NODE_H_
