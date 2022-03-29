#include "node2deploy.h"
#include "src/server_common/rpc_closure.h"
#include "src/server_common/rpc_closure.h"
#include "src/server_common/rpc_closure.h"
#include "src/server_common/rpc_closure.h"

///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"

#include "src/deploy_server.h"
#include "src/game_logic/entity/entity.h"
#include "src/deploy_variable.h"
#include "src/return_code/error_code.h"

///<<< END WRITING YOUR CODE

using namespace common;
namespace deploy{
///<<< BEGIN WRITING YOUR CODE
void DeployServiceImpl::LoadRegionDeploy(uint32_t region_id,
	::region_server_db* response)
	{
	std::string where_case = std::to_string(region_id) + " = id  ";
	db_->LoadOne(*response, where_case);
	}
///<<< END WRITING YOUR CODE

///<<<rpc begin
void DeployServiceImpl::ServerInfo(::google::protobuf::RpcController* controller,
    const deploy::ServerInfoRequest* request,
    deploy::ServerInfoResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE ServerInfo
	auto group_id = request->group();
	auto& servers_deploy = *response->mutable_info();
	if (group_id > 0)
	{
		std::string where_case = std::to_string(group_id) + " = id  ";
		db_->LoadOne(*servers_deploy.mutable_database_info(), where_case);
		db_->LoadOne(*servers_deploy.mutable_login_info(), where_case);
		db_->LoadOne(*servers_deploy.mutable_master_info(), where_case);
		db_->LoadOne(*servers_deploy.mutable_gateway_info(), where_case);
		db_->LoadOne(*servers_deploy.mutable_redis_info(), where_case);
	}
	else
	{
		std::string where_case = std::to_string(request->region_id()) + " = region_id  ";
		db_->LoadAll<::master_server_db>(*response->mutable_region_masters(), where_case);
	}
	LoadRegionDeploy(request->region_id(), servers_deploy.mutable_regin_info());
///<<< END WRITING YOUR CODE ServerInfo
}

void DeployServiceImpl::StartGS(::google::protobuf::RpcController* controller,
    const deploy::StartGSRequest* request,
    deploy::StartGSResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE StartGS
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

	g_deploy_server->reuse_game_id().Emplace(ip_port.toIpPort(), node_id);
	g_deploy_server->SaveGSDb();
	//g_deploy_server->LogReuseInfo();
///<<< END WRITING YOUR CODE StartGS
}

void DeployServiceImpl::StartRegionServer(::google::protobuf::RpcController* controller,
    const deploy::RegionInfoRequest* request,
    deploy::RegionInfoResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE StartRegionServer
	LoadRegionDeploy(request->region_id(), response->mutable_info());
///<<< END WRITING YOUR CODE StartRegionServer
}

void DeployServiceImpl::LoadRegionDeploy(::google::protobuf::RpcController* controller,
    const deploy::RegionInfoRequest* request,
    deploy::RegionInfoResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE LoadRegionDeploy
	LoadRegionDeploy(request->region_id(), response->mutable_info());
///<<< END WRITING YOUR CODE LoadRegionDeploy
}

///<<<rpc end
}// namespace deploy
