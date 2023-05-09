#include "deploy_service_replied.h"

#include "src/game_config/deploy_json.h"
#include "src/gate_server.h"
#include "src/pb/pbc/deploy_service_service.h"
#include "src/thread_local/gate_thread_local_storage.h"

extern ServerSequence32 g_server_sequence_;


void OnLoginNodeInfoReplied(const TcpConnectionPtr& conn, const GruoupLoginNodeResponsePtr& replied, Timestamp timestamp)
{
    for (const auto& replied_it : replied->login_db().login_nodes())
    {
        //todo
        if (replied_it.id() != 1)
        {
            continue;
        }
        auto& login_info = replied_it;
        auto it = gate_tls.login_nodes().emplace(login_info.id(), LoginNode());
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
    auto& server_node_data = replied->info();
    g_gate_node->set_servers_info_data(server_node_data);
    g_server_sequence_.set_node_id(g_gate_node->gate_node_id());
    g_gate_node->StartServer();   
}
