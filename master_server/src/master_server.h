#ifndef MASTER_SERVER_MASTER_SERVER_H_
#define MASTER_SERVER_MASTER_SERVER_H_

#include "entt/src/entt/entity/registry.hpp"

#include "src/event/event.h"
#include "src/service/ms_node.h"
#include "src/redis_client/redis_client.h"
#include "src/service/ms_node.h"
#include "src/network/deploy_rpcclient.h"
#include "src/network/rpc_closure.h"
#include "src/network/rpc_connection_event.h"
#include "src/network/rpc_server.h"
#include "src/network/server_component.h"

#include "deploy_node.pb.h"
#include "ms2db.pb.h"

    class MasterServer : muduo::noncopyable, public common::Receiver<MasterServer>
    {
    public:
        using RedisClientPtr = common::RedisClientPtr;
        using RpcServerPtr = std::shared_ptr<muduo::net::RpcServer>;
        using LoginStubms2db = common::RpcStub<ms2db::LoginService_Stub>;

        MasterServer(muduo::net::EventLoop* loop);           

        inline RedisClientPtr& redis_client() { return redis_; }
        inline uint32_t master_node_id()const { return serverinfos_.master_info().id(); }

        void Init();

        void ConnectDeploy();

        using ServerInfoRpcClosure = common::NormalClosure<deploy::ServerInfoRequest,
            deploy::ServerInfoResponse>;
        using ServerInfoRpcRC = std::shared_ptr<ServerInfoRpcClosure>;
        void StartServer(ServerInfoRpcRC cp);

        void DoGateConnectGs(entt::entity gs, entt::entity gate);
        void OnGsNodeStart(entt::entity gs);

        void receive(const common::OnConnected2ServerEvent& es);
        void receive(const common::OnBeConnectedEvent& es);

    private:      

        void InitConfig();
        void InitGlobalEntities();

        muduo::net::EventLoop* loop_{ nullptr };
        RedisClientPtr redis_;
        RpcServerPtr server_;

        common::RpcClientPtr deploy_rpc_client_;
        deploy::DeployStub deploy_stub_;

        common::RpcClientPtr db_rpc_client_;
        LoginStubms2db msl2_login_stub_;

        msservice::MasterNodeServiceImpl node_service_impl_;
 
        servers_info_data serverinfos_;
    };

extern MasterServer* g_ms_node;

#endif//MASTER_SERVER_MASTER_SERVER_H_
