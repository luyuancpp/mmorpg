#ifndef LOGIN_SERVER_LOGIN_SERVER_H
#define LOGIN_SERVER_LOGIN_SERVER_H

#include "src/event/event.h"
#include "src/server_common/rpc_server.h"
#include "src/service/gw2l.h"
#include "src/server_common/deploy_rpcclient.h"
#include "src/server_common/rpc_closure.h"
#include "src/server_common/rpc_connection_event.h"
#include "src/redis_client/redis_client.h"

#include "node2deploy.pb.h"

#include "ms_node.pb.h"

namespace login
{
    class LoginServer : muduo::noncopyable, public common::Receiver<LoginServer>
    {
    public:
        using RedisClientPtr = common::RedisClientPtr;
        using RpcServerPtr = std::shared_ptr<muduo::net::RpcServer>;
        using LoginStubl2ms = common::RpcStub<msservice::MasterNodeService_Stub>;
        using LoginStubl2db = common::RpcStub<l2db::LoginService_Stub>;

        LoginServer(muduo::net::EventLoop* loop);
            
        RedisClientPtr& redis_client() { return redis_; }
        uint32_t login_node_id() const { return node_info_.id(); }

        void LoadConfig();

        void ConnectDeploy();

        void Start();

        using ServerInfoRpcClosure = common::NormalClosure<deploy::ServerInfoRequest,
            deploy::ServerInfoResponse>;
        using ServerInfoRpcRC = std::shared_ptr<ServerInfoRpcClosure>;
        void StartServer(ServerInfoRpcRC cp);

        void receive(const common::OnConnected2ServerEvent& es);

    private:
        muduo::net::EventLoop* loop_{ nullptr };
        
        RedisClientPtr redis_;
        RpcServerPtr server_;

        common::RpcClientPtr deploy_rpc_client_;
        deploy::DeployStub deploy_stub_;

        common::RpcClientPtr master_rpc_client_;
        LoginStubl2ms l2ms_login_stub_;

        common::RpcClientPtr db_rpc_client_;
        LoginStubl2db l2db_login_stub_;

        gw2l::LoginServiceImpl impl_;

        login_server_db node_info_;
    };
}

extern login::LoginServer* g_login_server;

#endif // LOGIN_SERVER_LOGIN_SERVER_H

