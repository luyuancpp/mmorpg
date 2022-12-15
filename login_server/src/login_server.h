#pragma once

#include "src/event/event.h"
#include "src/network/rpc_server.h"
#include "src/network/rpc_msg_route.h"

#include "src/network/rpc_client.h"
#include "src/network/rpc_connection_event.h"
#include "src/redis_client/redis_client.h"
#include "src/service/common_proto/login_service.h"


    class LoginServer : muduo::noncopyable, public Receiver<LoginServer>
    {
    public:
        using RpcServerPtr = std::shared_ptr<muduo::net::RpcServer>;

        LoginServer(muduo::net::EventLoop* loop);
            
        inline PbSyncRedisClientPtr& redis_client() { return redis_; }
        inline RpcClientPtr& controller_node() { return controller_session_; }
        inline RpcClientPtr& db_node() { return db_session_; }
        uint32_t login_node_id() const { return conf_info_.id(); }
		inline const NodeInfo& node_info()const { return node_info_;}
		inline const NodeInfo& database_node_info()const {return dababase_node_info_;}
        inline const NodeInfo& controller_node_info()const { return controller_node_info_; }

        void Init();

        void ConnectDeploy();

        void Start();

        void StartServer(const ::servers_info_data& info);

        void receive(const OnConnected2ServerEvent& es);

    private:
        muduo::net::EventLoop* loop_{ nullptr };
        
        PbSyncRedisClientPtr redis_;
        RpcServerPtr server_;

        RpcClientPtr deploy_session_;
        RpcClientPtr controller_session_;
        RpcClientPtr db_session_;

        LoginServiceImpl impl_;

        login_server_db conf_info_;
        NodeInfo node_info_;
        NodeInfo dababase_node_info_;
        NodeInfo controller_node_info_;
    };

extern LoginServer* g_login_node;


