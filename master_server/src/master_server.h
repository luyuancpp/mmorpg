#ifndef MASTER_SERVER_MASTER_SERVER_H_
#define MASTER_SERVER_MASTER_SERVER_H_

#include "src/event/event.h"
#include "src/login/service_l2ms.h"
#include "src/redis_client/redis_client.h"
#include "src/game/game_client.h"
#include "src/server_common/deploy_rpcclient.h"
#include "src/server_common/rpc_closure.h"
#include "src/server_common/rpc_connection_event.h"
#include "src/server_common/rpc_server.h"
#include "src/server_common/server_component.h"

#include "deploy.pb.h"
#include "l2ms.pb.h"
#include "ms2db.pb.h"

namespace master
{
    class MasterServer : muduo::noncopyable, public common::Receiver<MasterServer>
    {
    public:
        using RedisClientPtr = common::RedisClientPtr;
        using RpcServerPtr = std::shared_ptr<muduo::net::RpcServer>;
        using LoginStubms2db = common::RpcStub<ms2db::LoginService_Stub>;

        MasterServer(muduo::net::EventLoop* loop);           

        RedisClientPtr& redis_client() { return redis_; }

        bool IsGateClient(const std::string& peer_addr);
        bool IsGameClient(const std::string& peer_addr);

        void LoadConfig();

        void ConnectDeploy();

        void receive(const common::ClientConnectionES& es);
        void receive(const common::ServerConnectionES& es);

        using ServerInfoRpcClosure = common::RpcClosure<deploy::ServerInfoRequest,
            deploy::ServerInfoResponse>;
        using ServerInfoRpcRC = std::shared_ptr<ServerInfoRpcClosure>;
        void StartServer(ServerInfoRpcRC cp);

    private:
        void OnRpcClientConnectionConnect(const muduo::net::TcpConnectionPtr& conn);
        void OnRpcClientConnectionDisConnect(const muduo::net::TcpConnectionPtr& conn);

        muduo::net::EventLoop* loop_{ nullptr };
        RedisClientPtr redis_;
        RpcServerPtr server_;

        common::RpcClientPtr deploy_rpc_client_;
        deploy::DeployRpcStub deploy_stub_;

        common::RpcClientPtr db_rpc_client_;
        LoginStubms2db msl2_login_stub_;

        l2ms::LoginServiceImpl impl_;

        master::GameClient game_client_;

        ::google::protobuf::RepeatedPtrField< ::serverinfo_database > info_;

        common::RpcServerConnectionPtr gate_client_;
    };
}//namespace master

#endif//MASTER_SERVER_MASTER_SERVER_H_
