#ifndef LOGIN_SERVER_LOGIN_SERVER_H
#define LOGIN_SERVER_LOGIN_SERVER_H

#include "src/event/event.h"
#include "src/server_common/rpc_server.h"
#include "src/gateway/service.h"
#include "src/server_common/deploy_rpcclient.h"
#include "src/server_common/rpc_closure.h"
#include "src/server_common/rpc_connection_event.h"
#include "src/redis_client/redis_client.h"

#include "deploy.pb.h"

namespace login
{
    class LoginServer : muduo::noncopyable, public common::Receiver<LoginServer>
    {
    public:
        using RedisClientPtr = common::RedisClientPtr;
        using RprServerPtr = std::shared_ptr<muduo::net::RpcServer>;
        using LoginStubl2ms = common::RpcStub<l2ms::LoginService_Stub>;
        using LoginStubl2db = common::RpcStub<l2db::LoginService_Stub>;

        LoginServer(muduo::net::EventLoop* loop);
            
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

        common::RpcClientPtr master_rpc_client_;
        LoginStubl2ms l2ms_login_stub_;

        common::RpcClientPtr db_rpc_client_;
        LoginStubl2db l2db_login_stub_;

        gw2l::LoginServiceImpl impl_;
    };
}

#endif // LOGIN_SERVER_LOGIN_SERVER_H

