#include "gate_server.h"

#include "src/game_config/deploy_json.h"
#include "src/network/gs_node.h"
#include "src/network/login_node.h"
#include "src/network/server_component.h"
#include "src/pb/pbc/msgmap.h"

#include "gs_service.pb.h"

GateServer* g_gateway_server = nullptr; 

extern ServerSequence32 g_server_sequence_;

void GateServer::LoadConfig()
{
    GameConfig::GetSingleton().Load("game.json");
    DeployConfig::GetSingleton().Load("deploy.json");
}

void GateServer::Init()
{
    g_gateway_server = this;
    LoadConfig();
    InitMsgService();
    const auto& deploy_info = DeployConfig::GetSingleton().deploy_info();
    InetAddress deploy_addr(deploy_info.ip(), deploy_info.port());
    deploy_session_ = std::make_unique<RpcClient>(loop_, deploy_addr);
    deploy_session_->subscribe<RegisterStubEvent>(deploy_stub_);
    deploy_session_->subscribe<OnConnected2ServerEvent>(*this);
    deploy_session_->connect();
}

void GateServer::StartServer(ServerInfoRpc replied)
{
    serverinfo_data_ = replied->s_rp_->info();
    g_server_sequence_.set_node_id(gate_node_id());

    EventLoop::getEventLoopOfCurrentThread()->queueInLoop(
        [this]() ->void
        {
            LoginNodeInfoRpc rpc(std::make_shared<LoginNodeInfoRpc::element_type>());
            rpc->s_rq_.set_group_id(GameConfig::GetSingleton().config_info().group_id());
            deploy_stub_.CallMethod(
                &GateServer::LoginNoseInfoReplied,
                rpc,
                this,
                &deploy::DeployService_Stub::LoginNodeInfo);
        }
    );
    
    auto& controller_node_info = serverinfo_data_.controller_info();
    InetAddress master_addr(controller_node_info.ip(), controller_node_info.port());
    controller_node_session_ = std::make_unique<RpcClient>(loop_, master_addr);
    controller_node_session_->registerService(&node_service_impl_);
    controller_node_session_->subscribe<RegisterStubEvent>(controller_stub_);
    controller_node_session_->subscribe<OnConnected2ServerEvent>(*this);
    controller_node_session_->connect();        

    auto& myinfo = serverinfo_data_.gate_info();
    InetAddress gateway_addr(myinfo.ip(), myinfo.port());
    server_ = std::make_unique<TcpServer>(loop_, gateway_addr, "gateway");
    server_->setConnectionCallback(
        std::bind(&GateServer::OnConnection, this, _1));
    server_->setMessageCallback(
        std::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));
    server_->start();
}

void GateServer::LoginNoseInfoReplied(LoginNodeInfoRpc replied)
{
    auto& rsp = replied->s_rp_;
    for (const auto& it : rsp->login_db().login_nodes())
    {
        if (it.id() != 1)
        {
            continue;
        }
        ConnectLogin(it);
    }
}

void GateServer::ConnectLogin(const login_server_db& login_info)
{
    auto it = g_login_nodes.emplace(login_info.id(), LoginNode());
    if (!it.second)
    {
        LOG_ERROR << "login server connected" << login_info.DebugString();
        return;
    }
    InetAddress login_addr(login_info.ip(), login_info.port());
	auto& login_node = it.first->second;
	login_node.login_session_ = std::make_unique<RpcClientPtr::element_type>(loop_, login_addr);
	login_node.login_stub_ = std::make_unique<LoginNode::LoginStubPtr::element_type>();
	login_node.login_session_->subscribe<RegisterStubEvent>(*login_node.login_stub_);
    login_node.login_session_->connect();
}

void GateServer::receive(const OnConnected2ServerEvent& es)
{
    auto& conn = es.conn_;
   
    if (IsSameAddr(conn->peerAddress(), DeployConfig::GetSingleton().deploy_info()))
    {
        // started 
        if (nullptr != server_)//断线重连
        {
            return;
        }
		if (!conn->connected())
		{
			return;
		}
        EventLoop::getEventLoopOfCurrentThread()->queueInLoop(
            [this]() ->void
            {
                ServerInfoRpc rpc(std::make_shared<ServerInfoRpc::element_type>());
                rpc->s_rq_.set_group(GameConfig::GetSingleton().config_info().group_id());
                deploy_stub_.CallMethod(
                    &GateServer::StartServer,
                    rpc,
                    this,
                    &deploy::DeployService_Stub::ServerInfo);
            }
        );
    }
    else if (IsSameAddr(conn->peerAddress(), serverinfo_data_.controller_info()))
    {
		if (!conn->connected())
		{
			return;
		}
		EventLoop::getEventLoopOfCurrentThread()->queueInLoop(
            [this]() ->void
			{
				auto& controller_node_addr = controller_node_session_->local_addr();
				controllerservice::ConnectRequest request;
				request.mutable_rpc_client()->set_ip(controller_node_addr.toIp());
				request.mutable_rpc_client()->set_port(controller_node_addr.port());
				request.set_gate_node_id(gate_node_id());
				controller_stub_.CallMethod(request, &controllerservice::ControllerNodeService_Stub::OnGwConnect);
			}
        );
    }
    else
    {
        //todo 断线重连
        for (auto& it : g_gs_nodes)
        {
            if (!IsSameAddr(it.second.gs_session_->peer_addr(), conn->peerAddress()))
            {
                continue;
            }
            if (conn->connected())
            {
				auto& gs_session = it.second;
				EventLoop::getEventLoopOfCurrentThread()->queueInLoop(
					[this, &gs_session, &conn]() ->void
					{
						gsservice::ConnectRequest request;
						request.mutable_rpc_client()->set_ip(conn->localAddress().toIp());
						request.mutable_rpc_client()->set_port(conn->localAddress().port());
						request.set_gate_node_id(gate_node_id());
						gs_session.gs_stub_->CallMethod(request, &gsservice::GsService_Stub::GwConnectGs);
					}
				);
            }
            else
            { 
                //g_gs_nodes.erase()
            }
        }

        for (auto& it : g_login_nodes)
        {
            LOG_INFO << it.second.login_session_->peer_addr().toIpPort() << "," << conn->peerAddress().toIpPort();
			if (!IsSameAddr(it.second.login_session_->peer_addr(), conn->peerAddress()))
			{
				continue;
			}
            if (conn->connected())
            {
                
            }
            else
            {
                g_login_nodes.erase(it.first);
                break;
            }
        }
    }
}

