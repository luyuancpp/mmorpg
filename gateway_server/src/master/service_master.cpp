#include "service_master.h"

#include "src/game/game_client.h"
#include "src/gateway_server.h"

#include "gw2g.pb.h"

using namespace  gateway;
using namespace  common;

namespace ms2gw
{
    void Ms2gwServiceImpl::StartLogicServer(::google::protobuf::RpcController* controller,
        const ::ms2gw::StartLogicServerRequest* request, 
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)
    {
        InetAddress gameserver_addr(request->ip(), request->port());
        auto e = GameClient::GetSingleton().create();
        auto& c = GameClient::GetSingleton().emplace<RpcClientPtr>(e, 
            std::make_unique<RpcClient>(EventLoop::getEventLoopOfCurrentThread(), gameserver_addr));
        using Gw2gStubPtr = RpcStub<gw2g::Gw2gService_Stub>::MyType;
        auto& sc =  GameClient::GetSingleton().emplace<Gw2gStubPtr>(e,
            std::make_unique<RpcStub<gw2g::Gw2gService_Stub>>());
        c->subscribe<RegisterStubES>(*(sc.get()));
        c->connect();
        GameClient::GetSingleton().emplace<InetAddress>(e,
            gameserver_addr);

        gw2ms::ConnectedLogicRequest gw2msrequest;
        gw2msrequest.mutable_rpc_client()->set_ip(request->ip());
        gw2msrequest.mutable_rpc_client()->set_port(request->port());
        server_->gw2ms_stub().CallMethod(gw2msrequest, &gw2ms::Gw2msService_Stub::GwConnectLogic);

    }

    void Ms2gwServiceImpl::StopLogicServer(::google::protobuf::RpcController* controller, 
        const ::ms2gw::StopLogicServerRequest* request, 
        ::google::protobuf::Empty* response, 
        ::google::protobuf::Closure* done)
    {
        for (auto e : GameClient::GetSingleton().view<InetAddress>())
        {
            auto& c = GameClient::GetSingleton().get<InetAddress>(e);
            if (c.toIp() != request->ip() ||
                c.port() != request->port())
            {
                continue;
            }
            GameClient::GetSingleton().destroy(e);
            break;
        }
    }

}


