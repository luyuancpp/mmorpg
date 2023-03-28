#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "src/event/event.h"
#include "src/service/common_proto/controller_service.h"
#include "src/redis_client/redis_client.h"
#include "src/network/rpc_client.h"
#include "src/network/rpc_connection_event.h"
#include "src/network/rpc_server.h"
#include "src/network/server_component.h"

#include "deploy_service.pb.h"
#include "database_service.pb.h"
#include "logic_proto/lobby_scene.pb.h"

class ControllerServer : muduo::noncopyable, public Receiver<ControllerServer>
{
public:
	using PbSyncRedisClientPtr = PbSyncRedisClientPtr;
	using RpcServerPtr = std::shared_ptr<muduo::net::RpcServer>;


	ControllerServer(muduo::net::EventLoop* loop);

	inline PbSyncRedisClientPtr& redis_client() { return redis_; }
	inline uint32_t controller_node_id()const { return serverinfos_.controller_info().id(); }
	inline RpcClientPtr& lobby_node() { return lobby_session_; }
	inline const NodeInfo& node_info()const { return node_info_; }

	void Init();
	void LetGateConnect2Gs(entt::entity gs, entt::entity gate);

    void StartServer(const ::servers_info_data& info);

	void receive(const OnConnected2ServerEvent& es);
	void receive(const OnBeConnectedEvent& es);

private:

	void InitConfig();

	void Connect2Deploy();
	void Connect2Lobby();
	void Register2Lobby();

	muduo::net::EventLoop* loop_{ nullptr };
	PbSyncRedisClientPtr redis_;
	RpcServerPtr server_;

	RpcClientPtr deploy_session_;
	RpcClientPtr lobby_session_;
	RpcClientPtr db_session_;

	ControllerServiceImpl contoller_service_;

	NodeInfo node_info_;
	servers_info_data serverinfos_;
};

extern ControllerServer* g_controller_node;


