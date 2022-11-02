#pragma once
#include "deploy_service.pb.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/mysql_wrapper/mysql_database.h"

///<<< END WRITING YOUR CODE
	///<<< BEGIN WRITING YOUR CODE
	///<<< END WRITING YOUR CODE
class DeployServiceImpl : public deploy::DeployService{
public:
		///<<< BEGIN WRITING YOUR CODE
		using ServerId = uint32_t;
		using MysqlClientPtr = std::shared_ptr<MysqlDatabase>;
		using GSMap = std::unordered_map<uint32_t, ::game_server_db>;

		void set_player_mysql_client(MysqlClientPtr& ptr)
		{
			db_ = ptr;
		}

	private:
		void LoadLobbyDeploy(uint32_t region_id, ::region_server_db* response);

		MysqlClientPtr db_;
		GSMap logic_server_map_;
		///<<< END WRITING YOUR CODE
public:
    void ServerInfo(::google::protobuf::RpcController* controller,
        const deploy::ServerInfoRequest* request,
        deploy::ServerInfoResponse* response,
        ::google::protobuf::Closure* done)override;

    void StartGS(::google::protobuf::RpcController* controller,
        const deploy::StartGSRequest* request,
        deploy::StartGSResponse* response,
        ::google::protobuf::Closure* done)override;

    void StartRegionServer(::google::protobuf::RpcController* controller,
        const deploy::RegionRequest* request,
        deploy::RegionServerResponse* response,
        ::google::protobuf::Closure* done)override;

    void RegionServerInfo(::google::protobuf::RpcController* controller,
        const deploy::RegionRequest* request,
        deploy::RegionServerResponse* response,
        ::google::protobuf::Closure* done)override;

    void RegionInfo(::google::protobuf::RpcController* controller,
        const deploy::RegionRequest* request,
        deploy::RegionInfoResponse* response,
        ::google::protobuf::Closure* done)override;

    void LoginNodeInfo(::google::protobuf::RpcController* controller,
        const deploy::GroupLignRequest* request,
        deploy::GruoupLoginNodeResponse* response,
        ::google::protobuf::Closure* done)override;

    void SceneSqueueNodeId(::google::protobuf::RpcController* controller,
        const deploy::SceneSqueueRequest* request,
        deploy::SceneSqueueResponese* response,
        ::google::protobuf::Closure* done)override;

};