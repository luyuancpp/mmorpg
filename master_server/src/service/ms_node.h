#pragma once
#include "ms_node.pb.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/common_type/common_type.h"
#include "src/comp/ms_login_account_comp.h"
#include "src/game_logic/comp/account_comp.h"
#include "src/network/rpc_closure.h"

#include "gs_node.pb.h"
#include "gw_node.pb.h"
///<<< END WRITING YOUR CODE
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class MasterNodeServiceImpl : public msservice::MasterNodeService{
public:
///<<< BEGIN WRITING YOUR CODE
	using AccountMap = std::unordered_map<std::string, MSLoginAccount>;

    using Ms2gsEnterGsRpcRplied = NormalClosure<gsservice::EnterGsRequest, gsservice::EnterGsRespone>;
	void Ms2gsEnterGsReplied(Ms2gsEnterGsRpcRplied replied);

	using Ms2GsCoverPlayerRpcRplied = NormalClosure<gsservice::CoverPlayerRequest, gsservice::CoverPlayerRespone>;
	void Ms2gsCoverPlayerReplied(Ms2GsCoverPlayerRpcRplied replied);

    using Ms2GwPlayerEnterGsRpcReplied = NormalClosure<gwservice::PlayerEnterGsRequest, gwservice::PlayerEnterGsResponese>;
    void Ms2GwPlayerEnterGsReplied(Ms2GwPlayerEnterGsRpcReplied replied);

    void OnPlayerLongin(entt::entity player);
    void OnPlayerCover(entt::entity player);//顶号
private:
	AccountMap logined_accounts_;
///<<< END WRITING YOUR CODE
public:
    void StartGs(::google::protobuf::RpcController* controller,
        const msservice::StartGsRequest* request,
        msservice::StartGsResponse* response,
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

    void OnGsPlayerService(::google::protobuf::RpcController* controller,
        const msservice::PlayerNodeServiceRequest* request,
        msservice::PlayerMessageRespone* response,
        ::google::protobuf::Closure* done)override;

    void OnAddCrossServerScene(::google::protobuf::RpcController* controller,
        const msservice::AddCrossServerSceneRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

};