#ifndef MASTER_SERVER_MASTER_SERVER_H_
#define MASTER_SERVER_MASTER_SERVER_H_

#include "src/event/event.h"
#include "src/server_common/rpc_server.h"
#include "src/login/service.h"
#include "src/server_common/rpc_closure.h"
#include "src/server_common/rpc_connection_event.h"
#include "src/redis_client/redis_client.h"
#include "src/server_common/deploy_rpcclient.h"

#include "deploy.pb.h"
#include "l2ms.pb.h"
#include "ms2db.pb.h"

namespace master
{
    class MasterServer : muduo::noncopyable, public common::Receiver<MasterServer>
    {
    public:
        using RedisClientPtr = common::RedisClientPtr;
        using RprServerPtr = std::shared_ptr<muduo::net::RpcServer>;
        using LoginStubms2db = common::RpcStub<ms2db::LoginService_Stub>;

        MasterServer(muduo::net::EventLoop* loop);           

        RedisClientPtr& redis_client() { return redis_; }

        void LoadConfig();

        void ConnectDeploy();

        void Start();

        void receive(const common::ConnectionES& es);

        using ServerInfoRpcClosure = common::RpcClosure<deploy::ServerInfoRequest,
            deploy::ServerInfoResponse>;
        using ServerInfoRpcRC = std::shared_ptr<ServerInfoRpcClosure>;
        void StartServer(ServerInfoRpcRC cp);
    private:
        muduo::net::EventLoop* loop_{ nullptr };
        RedisClientPtr redis_;
        RprServerPtr server_;

        common::RpcClientPtr deploy_rpc_client_;
        deploy::DeployRpcStub deploy_stub_;

        common::RpcClientPtr db_rpc_client_;
        LoginStubms2db msl2_login_stub_;

        l2ms::LoginServiceImpl impl_;
    };
}//namespace master

#endif//MASTER_SERVER_MASTER_SERVER_H_
