#include "deploy_service_replied.h"

#include "src/game_config/deploy_json.h"
#include "src/gate_server.h"
#include "src/pb/pbc/service_method/deploy_servicemethod.h"
#include "src/thread_local/gate_thread_local_storage.h"

extern ServerSequence32 g_server_sequence_;


void OnLoginNodeInfoReplied(const TcpConnectionPtr& conn, const GruoupLoginNodeResponsePtr& replied, Timestamp timestamp)
{
    for (const auto& it : replied->login_db().login_nodes())
    {
        //todo
        if (it.id() != 1)
        {
            continue;
        }
        auto& login_info = it;
        auto it = gate_tls.login_nodes.emplace(login_info.id(), LoginNode());
        if (!it.second)
        {
            LOG_ERROR << "login server connected" << login_info.DebugString();
            return;
        }
        InetAddress login_addr(login_info.ip(), login_info.port());
        auto& login_node = it.first->second;
        login_node.login_session_ = std::make_unique<RpcClientPtr::element_type>(g_gate_node->loop(), login_addr);
        login_node.login_session_->connect();
    }
}

void OnServerInfoReplied(const TcpConnectionPtr& conn, const ServerInfoResponsePtr& replied, Timestamp timestamp)
{
    auto& serverinfo_data = replied->info();
    g_gate_node->set_servers_info_data(serverinfo_data);
    g_server_sequence_.set_node_id(g_gate_node->gate_node_id());

    EventLoop::getEventLoopOfCurrentThread()->queueInLoop(
        []() ->void
        {
            GroupLignRequest rq;
            rq.set_group_id(GameConfig::GetSingleton().config_info().group_id());
            g_gate_node->deploy_session()->CallMethod(DeployServiceLoginNodeInfoMethodDesc, &rq);
        }
    );
    g_gate_node->StartServer();   
}
