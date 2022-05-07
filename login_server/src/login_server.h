#ifndef LOGIN_SERVER_LOGIN_SERVER_H
#define LOGIN_SERVER_LOGIN_SERVER_H

#include "src/event/event.h"
#include "src/network/rpc_server.h"
#include "src/service/login_node.h"
#include "src/network/deploy_rpcclient.h"
#include "src/network/rpc_closure.h"
#include "src/network/rpc_connection_event.h"
#include "src/redis_client/redis_client.h"

#include "deploy_node.pb.h"

#include "ms_service.pb.h"

namespace login
{
    class LoginServer : muduo::noncopyable, public Receiver<LoginServer>
    {
    public:
        using RedisClientPtr = common::RedisClientPtr;
        using RpcServerPtr = std::shared_ptr<muduo::net::RpcServer>;
        using LoginStubl2ms = RpcStub<msservice::MasterNodeService_Stub>;
        using LoginStubl2db = RpcStub<dbservice::DbService_Stub>;

        LoginServer(muduo::net::EventLoop* loop);
            
        RedisClientPtr& redis_client() { return redis_; }
        uint32_t login_node_id() const { return node_info_.id(); }

        void Init();

        void ConnectDeploy();

        void Start();

        using ServerInfoRpcClosure = NormalClosure<deploy::ServerInfoRequest,
            deploy::ServerInfoResponse>;
        using ServerInfoRpcRC = std::shared_ptr<ServerInfoRpcClosure>;
        void StartServer(ServerInfoRpcRC cp);

        void receive(const OnConnected2ServerEvent& es);

    private:
        muduo::net::EventLoop* loop_{ nullptr };
        
        RedisClientPtr redis_;
        RpcServerPtr server_;

        RpcClientPtr deploy_rpc_client_;
        DeployStub deploy_stub_;

        RpcClientPtr master_rpc_client_;
        LoginStubl2ms l2ms_login_stub_;

        RpcClientPtr db_rpc_client_;
        LoginStubl2db l2db_login_stub_;

        LoginServiceImpl impl_;

        login_server_db node_info_;
    };
}

extern login::LoginServer* g_login_server;

#endif // LOGIN_SERVER_LOGIN_SERVER_H

