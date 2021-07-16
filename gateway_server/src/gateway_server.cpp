#include "gateway_server.h"

#include "src/game_config/game_config.h"
#include "src/server_common/deploy_rpcclient.h"
#include "src/server_type_id/server_type_id.h"

namespace gateway
{

void GatewayServer::LoadConfig()
{
    common::GameConfig::GetSingleton().Load("game.json");
    common::DeployConfig::GetSingleton().Load("deploy.json");
}

void GatewayServer::ConnectDeploy()
{
    const auto& deploy_info = common::DeployConfig::GetSingleton().deploy_param();
    InetAddress deploy_addr(deploy_info.ip(), deploy_info.port());
    deploy_rpc_client_ = std::make_unique<common::RpcClient>(loop_, deploy_addr);
    deploy_rpc_client_->emp()->subscribe<common::RegisterStubES>(deploy_stub_);
    deploy_rpc_client_->emp()->subscribe<common::ConnectionES>(*this);
    deploy_rpc_client_->connect();
}

void GatewayServer::receive(const common::ConnectionES& es)
{
    if (!es.conn_->connected())
    {
        return;
    }
    ServerInfoRpcRC cp(std::make_shared<ServerInfoRpcClosure>());
    cp->s_reqst_.set_group(common::GameConfig::GetSingleton().config_info().group_id());
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
    login_rpc_client_->emp()->subscribe<common::RegisterStubES>(gw2l_login_stub_);

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