#ifndef DEPLOY_SERVER_SRC_SERVICE_SERVICE_H_
#define DEPLOY_SERVER_SRC_SERVICE_SERVICE_H_
///<<< BEGIN WRITING YOUR CODE
#include "src/mysql_database/mysql_database.h"

#include "deploy.pb.h"
///<<< END WRITING YOUR CODE

namespace deploy
{
    ///<<< BEGIN WRITING YOUR CODE
    ///<<< END WRITING YOUR CODE
    class DeployServiceImpl : public DeployService
    {
    public:
        virtual void ServerInfo(::google::protobuf::RpcController* controller,
            const ::deploy::ServerInfoRequest* request,
            ::deploy::ServerInfoResponse* response,
            ::google::protobuf::Closure* done)override;

        virtual void StartGS(::google::protobuf::RpcController* controller,
            const ::deploy::StartGSRequest* request,
            ::deploy::StartGSResponse* response,
            ::google::protobuf::Closure* done)override;

        virtual void StartRegionServer(::google::protobuf::RpcController* controller,
            const ::deploy::RegionInfoRequest* request,
            ::deploy::RegionInfoResponse* response,
            ::google::protobuf::Closure* done)override;

		virtual void LoadRegionDeploy(::google::protobuf::RpcController* controller,
			const ::deploy::RegionInfoRequest* request,
			::deploy::RegionInfoResponse* response,
			::google::protobuf::Closure* done)override;
	///<<< BEGIN WRITING YOUR CODE
		using ServerId = uint32_t;
		using MysqlClientPtr = std::shared_ptr<common::MysqlDatabase>;
		using GSMap = std::unordered_map<uint32_t, ::game_server_db>;

		void set_player_mysql_client(MysqlClientPtr& ptr)
		{
			db_ = ptr;
		}

    private:
        void LoadRegionDeploy(uint32_t region_id, ::region_server_db* response);

        MysqlClientPtr db_;
        GSMap logic_server_map_;
        ///<<< END WRITING YOUR CODE
    };
}//namespace deploy

#endif//DEPLOY_SERVER_SRC_SERVICE_SERVICE_H_
