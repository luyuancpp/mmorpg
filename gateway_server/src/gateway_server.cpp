#include "gateway_server.h"

#include "src/game_config/game_config.h"
#include "src/server_common/deploy_rpcclient.h"
#include "src/server_common/server_type_id.h"

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
    deploy_rpc_client_->subscribe<common::RegisterStubES>(deploy_stub_);
    deploy_rpc_client_->subscribe<common::RpcClientConnectionES>(*this);
    deploy_rpc_client_->connect();
}

void GatewayServer::receive(const common::RpcClientConnectionES& es)
{
    if (!es.conn_->connected())
    {
        return;
    }
    
    if (IsSameAddr(es.conn_->peerAddress(), common::DeployConfig::GetSingleton().deploy_param()))
    {
        // started 
        if (nullptr != server_)
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
    else if (IsSameAddr(es.conn_->peerAddress(), serverinfo_database_.Get(common::SERVER_MASTER)))
    {
        gw2ms::ConnectRequest request;
        request.mutable_rpc_client()->set_ip(es.conn_->localAddress().toIp());
        request.mutable_rpc_client()->set_port(es.conn_->localAddress().port());
        gw2ms_stub_.CallMethod(request, &gw2ms::Gw2msService_Stub::GwConnectMaster);
    }
}

void GatewayServer::StartServer(ServerInfoRpcRC cp)
{
    serverinfo_database_ = cp->s_resp_->info();
    auto& login_info = serverinfo_database_.Get(common::SERVER_LOGIN);
    InetAddress login_addr(login_info.ip(), login_info.port());
    login_rpc_client_ = std::make_unique<common::RpcClient>(loop_, login_addr);
    login_rpc_client_->connect();
    login_rpc_client_->subscribe<common::RegisterStubES>(gw2l_login_stub_);

    auto& master_info = serverinfo_database_.Get(common::SERVER_MASTER);
    InetAddress master_addr(master_info.ip(), master_info.port());
    master_rpc_client_ = std::make_unique<common::RpcClient>(loop_, master_addr);
    master_rpc_client_->registerService(&ms2gw_service_impl_);
    master_rpc_client_->subscribe<common::RegisterStubES>(gw2ms_stub_);
    master_rpc_client_->subscribe<common::RpcClientConnectionES>(*this);
    master_rpc_client_->connect();        

    auto& myinfo = serverinfo_database_.Get(common::SERVER_GATEWAY);
    InetAddress gateway_addr(myinfo.ip(), myinfo.port());
    server_ = std::make_unique<TcpServer>(loop_, gateway_addr, "gateway");
    server_->setConnectionCallback(
        std::bind(&GatewayServer::OnConnection, this, _1));
    server_->setMessageCallback(
        std::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));
    server_->start();
}

}//namespace gateway