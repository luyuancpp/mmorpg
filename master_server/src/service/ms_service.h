#pragma once
#include "ms_service.pb.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/common_type/common_type.h"
#include "src/comp/account_player.h"
#include "src/game_logic/comp/account_comp.h"
#include "src/network/rpc_closure.h"

#include "gs_service.pb.h"
#include "gw_service.pb.h"
///<<< END WRITING YOUR CODE
class MasterNodeServiceImpl : public msservice::MasterNodeService{
public:
///<<< BEGIN WRITING YOUR CODE
	using AccountMap = std::unordered_map<std::string, uint64_t>;

    using Ms2GwPlayerEnterGsRpcReplied = NormalClosure<gwservice::PlayerEnterGsRequest, gwservice::PlayerEnterGsResponese>;
    void Ms2GwPlayerEnterGsReplied(Ms2GwPlayerEnterGsRpcReplied replied);

    void OnPlayerLongin(entt::entity player);

    Guid GetPlayerIdByConnId(uint64_t session_id);
    entt::entity GetPlayerByConnId(uint64_t session_id);

    void OnConnidEnterGame(entt::entity conn, Guid player_id);
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

    void AddCrossServerScene(::google::protobuf::RpcController* controller,
        const msservice::AddCrossServerSceneRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void EnterGsSucceed(::google::protobuf::RpcController* controller,
        const msservice::EnterGsSucceedRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

};