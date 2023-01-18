#include "deploy_service.h"
#include "src/network/rpc_msg_route.h"
///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"

#include "src/deploy_server.h"
#include "src/game_logic/thread_local/game_registry.h"
#include "src/deploy_variable.h"
#include "src/game_logic/tips_id.h"

ReuseId<uint32_t, std::unordered_set<uint32_t>, UINT32_MAX> g_scene_squeue_node_id;
using ServerId = uint32_t;

using GSMap = std::unordered_map<uint32_t, ::game_server_db>;
static MysqlClientPtr db_;
static GSMap logic_server_map_;

void LoadLobbyDeploy(uint32_t lobby_id,
	::lobby_server_db* response)
{
	std::string where_case = std::to_string(lobby_id) + " = id  ";
	db_->LoadOne(*response, where_case);
}

void set_player_mysql_client(MysqlClientPtr& ptr)
{
	db_ = ptr;
}
///<<< END WRITING YOUR CODE

	///<<<rpc begin
void DeployServiceImpl::ServerInfo(::google::protobuf::RpcController* controller,
    const ::ServerInfoRequest* request,
    ::ServerInfoResponse* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE 
	auto group_id = request->group();
	auto& servers_deploy = *response->mutable_info();

	std::string where_case = std::to_string(group_id) + " = id  ";
	db_->LoadOne(*servers_deploy.mutable_database_info(), where_case);
	db_->LoadOne(*servers_deploy.mutable_login_info(), where_case);
	db_->LoadOne(*servers_deploy.mutable_controller_info(), where_case);
	db_->LoadOne(*servers_deploy.mutable_gate_info(), where_case);
	db_->LoadOne(*servers_deploy.mutable_redis_info(), where_case);

	LoadLobbyDeploy(request->lobby_id(), servers_deploy.mutable_lobby_info());
///<<< END WRITING YOUR CODE 
}

void DeployServiceImpl::StartGS(::google::protobuf::RpcController* controller,
    const ::StartGSRequest* request,
    ::StartGSResponse* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE 
	auto& server_deploy = *response->mutable_my_info();
	auto& client_info = request->rpc_client();
	muduo::net::InetAddress ip_port(client_info.ip(), client_info.port());
	if (server_deploy.id() > 0)
	{
		g_deploy_server->reuse_game_id().Emplace(ip_port.toIpPort(), server_deploy.id());
		return;
	}
	server_deploy.set_ip(request->my_info().ip());
	uint32_t node_id = g_deploy_server->CreateGSId();
	LOG_INFO << "new server id " << node_id;
	server_deploy.set_id(node_id);
	server_deploy.set_port(node_id + kGSBeginPort);
	std::string where_case = std::to_string(request->group()) + " = id  ";
	db_->LoadOne(*response->mutable_redis_info(), where_case);
	g_deploy_server->reuse_game_id().Emplace(ip_port.toIpPort(), node_id);
	g_deploy_server->SaveGSDb();
		//g_deploy_server->LogReuseInfo();
///<<< END WRITING YOUR CODE 
}

void DeployServiceImpl::StartLobbyServer(::google::protobuf::RpcController* controller,
    const ::LobbyServerRequest* request,
    ::LobbyServerResponse* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE 
	LoadLobbyDeploy(request->lobby_id(), response->mutable_info());
///<<< END WRITING YOUR CODE 
}

void DeployServiceImpl::AcquireLobbyServerInfo(::google::protobuf::RpcController* controller,
    const ::LobbyServerRequest* request,
    ::LobbyServerResponse* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE 
	LoadLobbyDeploy(request->lobby_id(), response->mutable_info());
///<<< END WRITING YOUR CODE 
}

void DeployServiceImpl::AcquireLobbyInfo(::google::protobuf::RpcController* controller,
    const ::LobbyServerRequest* request,
    ::LobbyInfoResponse* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE 
	//获取大厅管理的controller节点
	std::string where_case = std::to_string(request->lobby_id()) + " = lobby_id  ";
	db_->LoadAll<controller_server_db>(*response->mutable_lobby_controllers(), where_case);
///<<< END WRITING YOUR CODE 
}

void DeployServiceImpl::LoginNodeInfo(::google::protobuf::RpcController* controller,
    const ::GroupLignRequest* request,
    ::GruoupLoginNodeResponse* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE 
    std::string where_case = std::to_string(request->group_id()) + " = group_id  ";
    db_->LoadAll<login_server_db>(*response->mutable_login_db(), where_case);
///<<< END WRITING YOUR CODE 
}

void DeployServiceImpl::SceneSqueueNodeId(::google::protobuf::RpcController* controller,
    const ::SceneSqueueRequest* request,
    ::SceneSqueueResponese* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE 
	response->set_node_id(g_scene_squeue_node_id.Create());
///<<< END WRITING YOUR CODE 
}

///<<<rpc end
