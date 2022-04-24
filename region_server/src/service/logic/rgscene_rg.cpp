#include "rgscene_rg.h"
#include "src/network/rpc_closure.h"
///<<< BEGIN WRITING YOUR CODE

#include "muduo/base/Logging.h"

#include "src/game_config/mainscene_config.h"

#include "src/game_logic/game_registry.h"
#include "src/game_logic/scene/scene_factories.h"
#include "src/game_logic/scene/scene.h"
#include "src/network/server_component.h"
#include "src/network/gs_node.h"
#include "src/network/ms_node.h"

#include "gs_node.pb.h"
#include "ms_node.pb.h"

using namespace muduo;
using namespace muduo::net;

using GsStubPtr = std::unique_ptr <common::RpcStub<gsservice::GsService_Stub>>;
using MsStubPtr = std::unique_ptr <common::RpcStub<msservice::MasterNodeService_Stub>>;
///<<< END WRITING YOUR CODE

using namespace common;
namespace regionservcie{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void RgServiceImpl::StartCrossGs(::google::protobuf::RpcController* controller,
    const regionservcie::StartCrossGsRequest* request,
    regionservcie::StartCrossGsResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE StartCrossGs

	InetAddress rpc_client_peer_addr(request->rpc_client().ip(), request->rpc_client().port());
	InetAddress rpc_server_peer_addr(request->rpc_server().ip(), request->rpc_server().port());
	entt::entity gs_entity{ entt::null };
	for (auto e : reg.view<RpcServerConnection>())
	{
		if (reg.get<RpcServerConnection>(e).conn_->peerAddress().toIpPort() != rpc_client_peer_addr.toIpPort())
		{
			continue;
		}
		gs_entity = e;
		break;
	}
	if (gs_entity == entt::null)
	{
		//todo
		LOG_INFO << "game connection not found " << request->gs_node_id();
		return;
	}

	auto c = reg.get<RpcServerConnection>(gs_entity);
	GsNodePtr gs = std::make_shared<GsNode>(c.conn_);
	gs->node_info_.set_node_id(request->gs_node_id());
	gs->node_info_.set_node_type(kGsNode);
	MakeGSParam make_gs_p;
	make_gs_p.node_id_ = request->gs_node_id();
	AddMainSceneNodeCompnent(gs_entity, make_gs_p);
	reg.emplace<InetAddress>(gs_entity, rpc_server_peer_addr);
	reg.emplace<GsNodePtr>(gs_entity, gs);
	reg.emplace<GsStubPtr>(gs_entity, std::make_unique<GsStubPtr::element_type>(boost::any_cast<muduo::net::RpcChannelPtr>(c.conn_->getContext())));
	if (request->server_type() == kMainSceneServer)
	{
		auto& config_all = mainscene_config::GetSingleton().all();
		MakeGSSceneP create_scene_param;
		create_scene_param.server_entity_ = gs_entity;
		for (int32_t i = 0; i < config_all.data_size(); ++i)
		{
			create_scene_param.scene_confid_ = config_all.data(i).id();
			auto scene_entity = g_scene_sys->MakeScene2Gs(create_scene_param);
			if (!reg.valid(scene_entity))
			{
				continue;
			}
			response->add_scenes_info()->CopyFrom(reg.get<SceneInfo>(scene_entity));
		}
	}
	else
	{
		reg.remove<MainSceneServer>(gs_entity);
		reg.emplace<RoomSceneServer>(gs_entity);
	}
	g_gs_nodes.emplace(request->gs_node_id(), gs_entity);
	LOG_INFO << "game node connected " << request->gs_node_id();
///<<< END WRITING YOUR CODE StartCrossGs
}

void RgServiceImpl::StartMS(::google::protobuf::RpcController* controller,
    const regionservcie::StartMSRequest* request,
    regionservcie::StartMSResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE StartMS
	InetAddress rpc_client_peer_addr(request->rpc_client().ip(), request->rpc_client().port());
	InetAddress rpc_server_peer_addr(request->rpc_server().ip(), request->rpc_server().port());
	entt::entity ms_entity{ entt::null };
	for (auto e : reg.view<RpcServerConnection>())
	{
		if (reg.get<RpcServerConnection>(e).conn_->peerAddress().toIpPort() != rpc_client_peer_addr.toIpPort())
		{
			continue;
		}
		ms_entity = e;
		break;
	}
	if (ms_entity == entt::null)
	{
		//todo
		LOG_INFO << "ms id found: " << request->ms_node_id();
		return;
	}

	auto c = reg.get<RpcServerConnection>(ms_entity);
	MsNodePtr ms = reg.emplace<MsNodePtr>(ms_entity, std::make_shared<MsNodePtr::element_type>(c.conn_));
	ms->node_info_.set_node_id(request->ms_node_id());
	ms->node_info_.set_node_type(kMasterNode);
	reg.emplace<InetAddress>(ms_entity, rpc_server_peer_addr);
	reg.emplace<MsStubPtr>(ms_entity, std::make_unique<MsStubPtr::element_type>(boost::any_cast<muduo::net::RpcChannelPtr>(c.conn_->getContext())));
	g_ms_nodes.emplace(request->ms_node_id(), ms_entity);
	LOG_INFO << "ms node connected " << request->ms_node_id();
///<<< END WRITING YOUR CODE StartMS
}

///<<<rpc end
}// namespace regionservcie
