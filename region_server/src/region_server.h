#ifndef REGION_SERVER_SRC_REGION_SERVER_H_
#define REGION_SERVER_SRC_REGION_SERVER_H_

#include "muduo/net/EventLoop.h"

#include "src/event/event.h"
#include "src/network/deploy_rpcclient.h"
#include "src/network/rpc_closure.h"
#include "src/network/rpc_server.h"
#include "src/service/logic/rgscene_rg.h"

namespace region
{
class RegionServer : muduo::noncopyable, public Receiver<RegionServer>
{
public:
    using RpcServerPtr = std::shared_ptr<muduo::net::RpcServer>;
    using DeployStub = RpcStub<deploy::DeployService_Stub>;
    RegionServer(muduo::net::EventLoop* loop);

    void Init();

    void ConnectDeploy();

    using RegionInfoRpcClosure = NormalClosure<deploy::RegionRequest,
        deploy::RegionServerResponse>;
    using RegionInfoRpcRpcRC = std::shared_ptr<RegionInfoRpcClosure>;
    void StartServer(RegionInfoRpcRpcRC cp);

    void receive(const OnConnected2ServerEvent& es);
    void receive(const OnBeConnectedEvent& es);
private:
    muduo::net::EventLoop* loop_{ nullptr };
    RpcServerPtr server_;

    RpcClientPtr deploy_rpc_client_;
    DeployStub deploy_stub_;

    RgServiceImpl impl_;
};
}

extern region::RegionServer* g_region_server;

#endif//REGION_SERVER_SRC_REGION_SERVER_H_


