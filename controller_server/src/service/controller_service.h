#pragma once
#include "controller_service.pb.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/common_type/common_type.h"
#include "src/comp/account_player.h"
#include "src/game_logic/comp/account_comp.h"
#include "src/network/rpc_closure.h"

#include "gs_service.pb.h"
#include "gw_service.pb.h"
///<<< END WRITING YOUR CODE
class ControllerNodeServiceImpl : public controllerservice::ControllerNodeService{
public:
///<<< BEGIN WRITING YOUR CODE
private:
	using AccountSessionMap = std::unordered_map<std::string, uint64_t>;

    using Ms2GwPlayerEnterGsRpc = NormalClosure<gwservice::PlayerEnterGsRequest, gwservice::PlayerEnterGsResponese>;
    void OnGateUpdatePlayerGsReplied(Ms2GwPlayerEnterGsRpc replied);

    Guid GetPlayerIdByConnId(uint64_t session_id);
    entt::entity GetPlayerByConnId(uint64_t session_id);    

    void OnSessionEnterGame(entt::entity conn, Guid player_id);

    void InitPlayerSession(entt::entity player, uint64_t session_id);

	AccountSessionMap logined_accounts_sesion_;
///<<< END WRITING YOUR CODE
public:
    void StartGs(::google::protobuf::RpcController* controller,
        const controllerservice::StartGsRequest* request,
        controllerservice::StartGsResponse* response,
        ::google::protobuf::Closure* done)override;

    void OnGwConnect(::google::protobuf::RpcController* controller,
        const controllerservice::ConnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void OnGwLeaveGame(::google::protobuf::RpcController* controller,
        const controllerservice::LeaveGameRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void OnGwPlayerService(::google::protobuf::RpcController* controller,
        const controllerservice::ClientMessageRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void OnGwDisconnect(::google::protobuf::RpcController* controller,
        const controllerservice::DisconnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void OnLsLoginAccount(::google::protobuf::RpcController* controller,
        const controllerservice::LoginAccountRequest* request,
        controllerservice::LoginAccountResponse* response,
        ::google::protobuf::Closure* done)override;

    void OnLsEnterGame(::google::protobuf::RpcController* controller,
        const controllerservice::EnterGameRequest* request,
        controllerservice::EnterGameResponese* response,
        ::google::protobuf::Closure* done)override;

    void OnLsLeaveGame(::google::protobuf::RpcController* controller,
        const controllerservice::LsLeaveGameRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void OnLsDisconnect(::google::protobuf::RpcController* controller,
        const controllerservice::LsDisconnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void OnGsPlayerService(::google::protobuf::RpcController* controller,
        const controllerservice::PlayerNodeServiceRequest* request,
        controllerservice::PlayerMessageRespone* response,
        ::google::protobuf::Closure* done)override;

    void AddCrossServerScene(::google::protobuf::RpcController* controller,
        const controllerservice::AddCrossServerSceneRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void EnterGsSucceed(::google::protobuf::RpcController* controller,
        const controllerservice::EnterGsSucceedRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

};