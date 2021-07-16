#include "gateway_server.h"

#include "src/game_config/game_config.h"
#include "src/net/deploy/rpcclient/deploy_rpcclient.h"
#include "src/server_type_id/server_type_id.h"

namespace gateway
{

void GatewayServer::LoadConfig()
{
    common::GameConfig::GetSingleton().Load("game.json");
}

void GatewayServer::ConnectDeploy()
{
    const auto& deploy_info = common::GameConfig::GetSingleton().deploy_server();
    InetAddress deploy_addr(deploy_info.host_name(), deploy_info.port());
    deploy_rpc_client_ = std::make_unique<common::RpcClient>(loop_, deploy_addr);
    deploy_rpc_client_->emp()->subscribe<common::RegisterStubEvent>(deploy_stub_);
    deploy_rpc_client_->emp()->subscribe<common::ConnectionEvent>(*this);
    deploy_rpc_client_->connect();
}

void GatewayServer::receive(const common::ConnectionEvent& es)
{
    if (!es.conn_->connected())
    {
        return;
    }
    ServerInfoRpcRC cp(std::make_shared<ServerInfoRpcClosure>());
    cp->s_reqst_.set_group(1);
    deploy_stub_.CallMethod(
        &GatewayServer::StartServer,
        cp,
        this,
        &deploy::DeployService_Stub::ServerInfo);
}


void GatewayServer::StartServer(ServerInfoRpcRC cp)
{
    auto& login_info = cp->s_resp_->info(common::SERVER_LOGIN);
    InetAddress login_addr(login_info.ip(), login_info.port());

    login_rpc_client_ = std::make_unique<common::RpcClient>(loop_, login_addr);
    login_rpc_client_->connect();
    login_rpc_client_->emp()->subscribe<common::RegisterStubEvent>(login_stub_gw2l_);

    auto& myinfo = cp->s_resp_->info(common::SERVER_GATEWAY);
    InetAddress gateway_addr(myinfo.ip(), myinfo.port());
    server_ = std::make_unique<TcpServer>(loop_, gateway_addr, "gateway");
    server_->setConnectionCallback(
        std::bind(&GatewayServer::OnConnection, this, _1));
    server_->setMessageCallback(
        std::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));
    server_->start();
}

}//namespace gateway