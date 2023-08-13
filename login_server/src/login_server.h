#pragma once

#include "src/network/rpc_server.h"

#include "src/handler/login_service_handler.h"
#include "src/network/rpc_client.h"
#include "src/network/rpc_connection_event.h"
#include "src/redis_client/redis_client.h"

class LoginServer : muduo::noncopyable
{
public:
	using RpcServerPtr = std::shared_ptr<muduo::net::RpcServer>;

	LoginServer(muduo::net::EventLoop* loop);
	~LoginServer();

	inline RpcClientPtr& controller_node() { return controller_session_; }
	inline RpcClientPtr& db_node() { return db_session_; }
	uint32_t login_node_id() const { return conf_info_.id(); }
	inline const NodeInfo& node_info()const { return node_info_; }
	inline const NodeInfo& database_node_info()const { return database_node_info_; }
	inline const NodeInfo& controller_node_info()const { return controller_node_info_; }
    inline LoginServiceHandler& login_handler(){return  handler_;}
	void Init();

	void ConnectDeploy();

	void Start();

	void StartServer(const ::servers_info_data& info);

	void Receive1(const OnConnected2ServerEvent& es) const;
	void Receive2(const OnBeConnectedEvent& es);

private:
	muduo::net::EventLoop* loop_{ nullptr };

	RpcServerPtr server_;

	RpcClientPtr deploy_session_;
	RpcClientPtr controller_session_;
	RpcClientPtr db_session_;

	LoginServiceHandler handler_;

	login_server_db conf_info_;
	NodeInfo node_info_;
	NodeInfo database_node_info_;
	NodeInfo controller_node_info_;
};

extern LoginServer* g_login_node;


