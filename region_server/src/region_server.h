#ifndef REGION_SERVER_SRC_REGION_SERVER_H_
#define REGION_SERVER_SRC_REGION_SERVER_H_

#include "muduo/net/EventLoop.h"

#include "src/event/event.h"
#include "src/server_common/deploy_rpcclient.h"
#include "src/server_common/rpc_closure.h"
#include "src/server_common/rpc_server.h"

namespace region
{
class RegionServer : muduo::noncopyable, public common::Receiver<RegionServer>
{
public:
    using RpcServerPtr = std::shared_ptr<muduo::net::RpcServer>;
    using DeployStub = common::RpcStub<deploy::DeployService_Stub>;
    RegionServer(muduo::net::EventLoop* loop);

    void Init();

    void ConnectDeploy();

    using RegionInfoRpcClosure = common::NormalClosure<deploy::RegionInfoRequest,
        deploy::RegionInfoResponse>;
    using RegionInfoRpcRpcRC = std::shared_ptr<RegionInfoRpcClosure>;
    void StartServer(RegionInfoRpcRpcRC cp);

    void receive(const common::RpcClientConnectionEvent& es);
    void receive(const common::ServerConnectionEvent& es);
private:
    muduo::net::EventLoop* loop_{ nullptr };
    RpcServerPtr server_;

    common::RpcClientPtr deploy_rpc_client_;
    deploy::DeployStub deploy_stub_;
};
}

extern region::RegionServer* g_region_server;

#endif//REGION_SERVER_SRC_REGION_SERVER_H_


