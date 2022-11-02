#pragma once

#include "src/event/event.h"
#include "src/network/rpc_server.h"
#include "src/service/login_service.h"
#include "src/network/rpc_closure.h"
#include "src/network/rpc_connection_event.h"
#include "src/redis_client/redis_client.h"

#include "deploy_service.pb.h"

#include "controller_service.pb.h"

namespace login
{
    class LoginServer : muduo::noncopyable, public Receiver<LoginServer>
    {
    public:
        using RpcServerPtr = std::shared_ptr<muduo::net::RpcServer>;
        using LoginStubController = RpcStub<controllerservice::ControllerNodeService_Stub>;
        using LoginStubl2db = RpcStub<dbservice::DbService_Stub>;

        LoginServer(muduo::net::EventLoop* loop);
            
        PbSyncRedisClientPtr& redis_client() { return redis_; }
        uint32_t login_node_id() const { return node_info_.id(); }

        void Init();

        void ConnectDeploy();

        void Start();

		using ServerInfoRpc = std::shared_ptr<NormalClosure<deploy::ServerInfoRequest,
			deploy::ServerInfoResponse>>;
        void StartServer(ServerInfoRpc replied);

        void receive(const OnConnected2ServerEvent& es);

    private:
        muduo::net::EventLoop* loop_{ nullptr };
        
        PbSyncRedisClientPtr redis_;
        RpcServerPtr server_;

        RpcClientPtr deploy_rpc_client_;
        RpcStub<deploy::DeployService_Stub> deploy_stub_;

        RpcClientPtr controller_client_;
        LoginStubController controller_login_stub_;

        RpcClientPtr db_rpc_client_;
        LoginStubl2db l2db_login_stub_;

        LoginServiceImpl impl_;

        login_server_db node_info_;
    };
}

extern login::LoginServer* g_login_server;


