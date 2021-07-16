#include "gateway_server.h"

#include "src/game_config/game_config.h"
#include "src/net/deploy/rpcclient/deploy_rpcclient.h"
#include "src/rpc_closure_param/rpc_closure.h"
#include "src/rpc_closure_param/rpc_connection_event.h"
#include "src/server_type_id/server_type_id.h"

using namespace gateway;

class DepolyReplied : public common::Receiver<DepolyReplied>
{
public:
    using GatewayServerPtr = std::shared_ptr<GatewayServer>;

    DepolyReplied(muduo::net::EventLoop* loop)
        :  loop_(loop)
    {
        deploy::DeployRpcClient::GetSingleton()->emp()->subscribe<common::DeployConnectionEvent>(*this);
        deploy::ServerInfoRpcStub::GetSingleton();
    }

    void receive(const common::ConnectionEvent& es)
    {
        if (!es.conn_->connected())
        {
            return;
        }
        ServerInfoRpcRC cp(std::make_shared<ServerInfoRpcClosure>());
        cp->s_reqst_.set_group(1);
        deploy::ServerInfoRpcStub::GetSingleton().CallMethod(
            &DepolyReplied::StartServer,
            cp,
            this,
            &deploy::DeployService_Stub::ServerInfo);
    }

    using ServerInfoRpcClosure = common::RpcClosure<deploy::ServerInfoRequest,
        deploy::ServerInfoResponse>;
    using ServerInfoRpcRC = std::shared_ptr<ServerInfoRpcClosure>;
    void StartServer(ServerInfoRpcRC cp)
    {

        auto& login_info = cp->s_resp_->info(common::SERVER_LOGIN);
        InetAddress login_addr(login_info.ip(), login_info.port());

        auto& myinfo = cp->s_resp_->info(common::SERVER_GATEWAY);
        InetAddress gateway_addr(myinfo.ip(), myinfo.port());

        server = std::make_shared<GatewayServer>(loop_, gateway_addr);
        LoginRpcClient::Connect(loop_, login_addr);
        LoginRpcStub::GetSingleton();
        server->Start();
    }
    
private:
    muduo::net::EventLoop* loop_{ nullptr };
    GatewayServerPtr server;
};



int main(int argc, char* argv[])
{
    common::GameConfig::GetSingleton().Load("game.json");
    const auto& deploy_info = common::GameConfig::GetSingleton().deploy_server();

    EventLoop loop;
    InetAddress deploy_addr(deploy_info.host_name(), deploy_info.port());
    deploy::DeployRpcClient::Connect(&loop, deploy_addr);

    DepolyReplied dp(&loop);
        
    loop.loop();
    return 0;
}