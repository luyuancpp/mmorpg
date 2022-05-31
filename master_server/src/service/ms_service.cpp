#include "ms_service.h"
#include "src/network/rpc_closure.h"
///<<< BEGIN WRITING YOUR CODE
/// #include "muduo/base/Logging.h"
#include "muduo/net/InetAddress.h"

#include "src/game_config/mainscene_config.h"

#include "src/network/gate_node.h"
#include "src/network/gs_node.h"
#include "src/network/player_session.h"
#include "src/game_logic/scene/scene_factories.h"
#include "src/game_logic/comp/scene_comp.h"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/scene/servernode_system.h"
#include "src/master_server.h"
#include "src/comp/player_list.h"
#include "src/network/message_system.h"
#include "src/network/gate_session.h"
#include "src/pb/pbc/msgmap.h"
#include "src/game_logic/tips_id.h"
#include "src/game_logic/scene/scene.h"
#include "src/network/node_info.h"
#include "src/service/logic/player_service.h"
#include "src/system/player_scene_system.h"
#include "src/system/player_common_system.h"
#include "src/network/server_component.h"
#include "src/network/session.h"

#include "gs_service.pb.h"
#include "logic_proto/scene_normal.pb.h"
#include "component_proto/ms_player_comp.pb.h"

using GsStubPtr = std::unique_ptr<RpcStub<gsservice::GsService_Stub>>;
using GwStub = RpcStub<gwservice::GwNodeService_Stub>;

std::size_t kMaxPlayerSize = 1000;

void MasterNodeServiceImpl::Ms2GwPlayerEnterGsReplied(Ms2GwPlayerEnterGsRpcReplied replied)
{
	auto it = g_gate_sessions.find(replied.s_rq_.session_id());
	if (it == g_gate_sessions.end())
	{
		LOG_ERROR << "conn not found " << replied.s_rq_.session_id();
		return;
	}
    /*auto player = registry.get<EntityPtr>(it->second);
    if (entt::null == player)
    {
        LOG_ERROR << "player not found " << replied.s_rq_.player_id();
        return;
    }
    g_player_common_sys.OnLogin(player);*/
}

void MasterNodeServiceImpl::OnPlayerLongin(entt::entity player)
{
	//ms 的login先调用，通知gs去调用
	//顶号
}

Guid MasterNodeServiceImpl::GetPlayerIdByConnId(uint64_t session_id)
{
    auto cit = g_gate_sessions.find(session_id);
    if (cit == g_gate_sessions.end())
    {
        return kInvalidGuid;
    }
    auto p_try_player = registry.try_get<EntityPtr>(cit->second);
    if (nullptr == p_try_player)
    {
        return kInvalidGuid;
    }
    auto player_id = registry.get<Guid>(*p_try_player);
	return kInvalidGuid;
}

entt::entity MasterNodeServiceImpl::GetPlayerByConnId(uint64_t session_id)
{
    auto cit = g_gate_sessions.find(session_id);
    if (cit == g_gate_sessions.end())
    {
		return entt::null;
    }
    auto p_try_player = registry.try_get<EntityPtr>(cit->second);
    if (nullptr == p_try_player)
    {
        return entt::null;
    }
    return (*p_try_player);
}

void MasterNodeServiceImpl::OnConnidEnterGame(entt::entity conn, Guid player_id)
{
    registry.emplace<EntityPtr>(conn, PlayerList::GetSingleton().GetPlayerPtr(player_id));
    registry.emplace<Guid>(conn, player_id);
}

///<<< END WRITING YOUR CODE

///<<<rpc begin
void MasterNodeServiceImpl::StartGs(::google::protobuf::RpcController* controller,
    const msservice::StartGsRequest* request,
    msservice::StartGsResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	response->set_master_node_id(g_ms_node->master_node_id());
	InetAddress rpc_client_peer_addr(request->rpc_client().ip(), request->rpc_client().port());
	InetAddress rpc_server_peer_addr(request->rpc_server().ip(), request->rpc_server().port());
	entt::entity gs_entity{ entt::null };
	for (auto e : registry.view<RpcServerConnection>())
	{
		if (registry.get<RpcServerConnection>(e).conn_->peerAddress().toIpPort() != rpc_client_peer_addr.toIpPort())
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

	auto c = registry.get<RpcServerConnection>(gs_entity);
	GsNodePtr gs = std::make_shared<GsNode>(c.conn_);
	gs->node_info_.set_node_id(request->gs_node_id());
	gs->node_info_.set_node_type(kGsNode);
	MakeGSParam make_gs_p;
	make_gs_p.node_id_ = request->gs_node_id();
	AddMainSceneNodeCompnent(gs_entity, make_gs_p);
	registry.emplace<InetAddress>(gs_entity, rpc_server_peer_addr);//为了停掉gs，或者gs断线用
	registry.emplace<GsNodePtr>(gs_entity, gs);
	registry.emplace<GsStubPtr>(gs_entity, std::make_unique<GsStubPtr::element_type>(boost::any_cast<muduo::net::RpcChannelPtr>(c.conn_->getContext())));
	if (request->server_type() == kMainSceneServer)
	{
		auto& config_all = mainscene_config::GetSingleton().all();
		MakeGSSceneP create_scene_param;
		create_scene_param.server_ = gs_entity;
		for (int32_t i = 0; i < config_all.data_size(); ++i)
		{
			create_scene_param.scene_confid_ = config_all.data(i).id();
			auto scene_entity = ScenesSystem::GetSingleton().MakeScene2Gs(create_scene_param);
			if (!registry.valid(scene_entity))
			{
				continue;
			}
			response->add_scenes_info()->CopyFrom(registry.get<SceneInfo>(scene_entity));
		}
	}
	else
	{
		registry.remove<MainSceneServer>(gs_entity);
		registry.emplace<RoomSceneServer>(gs_entity);
	}

	for (auto e : registry.view<GateNodePtr>())
	{
		g_ms_node->DoGateConnectGs(gs_entity, e);
	}
	g_ms_node->AddGsNode(gs_entity);
	LOG_INFO << "game connected " << request->gs_node_id();
///<<< END WRITING YOUR CODE 
}

void MasterNodeServiceImpl::OnGwConnect(::google::protobuf::RpcController* controller,
    const msservice::ConnectRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	InetAddress rpc_client_peer_addr(request->rpc_client().ip(), request->rpc_client().port());
	entt::entity gate_entity{ entt::null };
	for (auto e : registry.view<RpcServerConnection>())
	{
		auto c = registry.get<RpcServerConnection>(e);
		auto& local_addr = c.conn_->peerAddress();
		if (local_addr.toIpPort() != rpc_client_peer_addr.toIpPort())
		{
			continue;
		}
		gate_entity = e;
		auto& gate_node = *registry.emplace<GateNodePtr>(gate_entity, std::make_shared<GateNode>(c.conn_));
		gate_node.node_info_.set_node_id(request->gate_node_id());
		gate_node.node_info_.set_node_type(kGateWayNode);
		g_gate_nodes.emplace(request->gate_node_id(), gate_entity);
        registry.emplace_or_replace<GwStub>(gate_entity, GwStub(boost::any_cast<muduo::net::RpcChannelPtr>(c.conn_->getContext())));
		break;
	}

	for (auto e : registry.view<GsNodePtr>())
	{
		g_ms_node->DoGateConnectGs(e, gate_entity);
	}
///<<< END WRITING YOUR CODE 
}

void MasterNodeServiceImpl::OnGwLeaveGame(::google::protobuf::RpcController* controller,
    const msservice::LeaveGameRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

void MasterNodeServiceImpl::OnGwPlayerService(::google::protobuf::RpcController* controller,
    const msservice::ClientMessageRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

void MasterNodeServiceImpl::OnGwDisconnect(::google::protobuf::RpcController* controller,
    const msservice::DisconnectRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	auto player = GetPlayerByConnId(request->session_id());
	if (entt::null == player)
	{
		return;
	}
	auto try_acount = registry.try_get<PlayerAccount>(player);
	if (nullptr != try_acount)
	{
		logined_accounts_.erase(**try_acount);
	}	
	auto& player_session = registry.get<PlayerSession>(player);
	auto it = g_gs_nodes.find(player_session.gs_node_id());
	//notice 异步过程 gate 先重连过来，然后断开才收到，也就是会把新来的连接又断了，极端情况，也要测试如果这段代码过去了，会有什么问题
	if (it == g_gs_nodes.end() ||  player_session.gate_node_id() != node_id(request->session_id()))
	{
		return;
	}
	auto player_id = registry.get<Guid>(player);
	gsservice::DisconnectRequest message;
	message.set_player_id(player_id);
	registry.get<GsStubPtr>(it->second)->CallMethod(
		message,
		&gsservice::GsService_Stub::Disconnect);
	PlayerList::GetSingleton().LeaveGame(player_id);
///<<< END WRITING YOUR CODE 
}

void MasterNodeServiceImpl::OnLsLoginAccount(::google::protobuf::RpcController* controller,
    const msservice::LoginAccountRequest* request,
    msservice::LoginAccountResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 

	auto cit = g_gate_sessions.find(request->session_id());
	if (cit == g_gate_sessions.end())
	{
		cit = g_gate_sessions.emplace(request->session_id(), EntityPtr()).first;
	}
	if (cit == g_gate_sessions.end())
	{
        response->mutable_error()->set_id(kRetLoginUnkonwError);
        return;
	}
	auto conn = cit->second;
    registry.emplace<PlayerAccount>(conn, std::make_shared<std::string>(request->account()));
    registry.emplace<AccountLoginNode>(conn, AccountLoginNode{request->session_id()});
	//todo 
	auto lit = logined_accounts_.find(request->account());
	if (PlayerList::GetSingleton().player_size() >= kMaxPlayerSize)
	{
		//如果登录的是新账号,满了得去排队,是账号排队，还是角色排队>???
		response->mutable_error()->set_id(kRetLoginAccountPlayerFull);
		return;
	}

	if (lit != logined_accounts_.end())
	{
		//如果不是同一个登录服务器,踢掉已经登录的账号
		if (lit->second != request->session_id())
		{

		}
		else//告诉客户端登录中
		{
			response->mutable_error()->set_id(kRetLoginIng);
		}
	}
	else
	{
		logined_accounts_.emplace(request->account(), request->session_id());
	}
///<<< END WRITING YOUR CODE 
}

void MasterNodeServiceImpl::OnLsEnterGame(::google::protobuf::RpcController* controller,
    const msservice::EnterGameRequest* request,
    msservice::EnterGameResponese* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	//todo正常或者顶号进入场景
	//todo 断线重连进入场景，断线重连分时间
    auto cit = g_gate_sessions.find(request->session_id());
	if (cit == g_gate_sessions.end())
	{
		LOG_ERROR << "connection not found " << request->session_id();
		return;
	}
	auto conn = cit->second;
	auto player_id = request->player_id();
	auto player = PlayerList::GetSingleton().GetPlayer(player_id);
	auto try_acount = registry.try_get<PlayerAccount>(player);
	if (nullptr != try_acount)
	{
		logined_accounts_.erase(**try_acount);
	}
	if (entt::null == player)
	{
		//把旧的connection 断掉
		PlayerList::GetSingleton().EnterGame(player_id, EntityPtr());
		OnConnidEnterGame(conn, player_id);
		player = PlayerList::GetSingleton().GetPlayer(player_id);
		registry.emplace<Guid>(player, player_id);
		registry.emplace<PlayerAccount>(player, registry.get<PlayerAccount>(cit->second));
		auto& player_session = registry.emplace<PlayerSession>(player);
		player_session.gate_session_.set_session_id(request->session_id());
		auto gate_it = g_gate_nodes.find(node_id(request->session_id()));
		if (gate_it != g_gate_nodes.end())
		{
			auto gate = registry.try_get<GateNodePtr>(gate_it->second);
			if (nullptr != gate)
			{
				player_session.gate_ = *gate;
			}
		}
		GetSceneParam getp;
		getp.scene_confid_ = 1;
		auto scene = ServerNodeSystem::GetMainSceneNotFull(getp);
		if (scene == entt::null)//找不到上次的场景，放到默认场景里面
		{
			// todo default
			LOG_INFO << "player " << player_id << " enter default secne";
		}
		auto* p_gs_data = registry.try_get<GsDataPtr>(scene);
		if (nullptr == p_gs_data)//找不到gs了，放到好的gs里面
		{
			// todo default
			LOG_INFO << "player " << player_id << " enter default secne";
		}
		auto& gs_data = *p_gs_data;
		player_session.gs_ = gs_data;
		auto it = g_gs_nodes.find(gs_data->node_id());
        EnterSceneParam ep;
        ep.enterer_ = player;
        ep.scene_ = scene;
        ScenesSystem::GetSingleton().EnterScene(ep);//顶号的时候已经在场景里面了
		if (it != g_gs_nodes.end())
		{			
			gsservice::EnterGsRequest message;
			message.set_player_id(player_id);
			message.set_session_id(request->session_id());
			message.set_ms_node_id(g_ms_node->master_node_id());
            auto& scene_info = registry.get<SceneInfo>(scene);
			message.mutable_scenes_info()->CopyFrom(scene_info);
			registry.get<GsStubPtr>(it->second)->CallMethod(message, &gsservice::GsService_Stub::EnterGs);
		}
	}
	else//顶号,断线重连 记得gate 删除 踢掉老gate,但是是同一个gate就不用了
	{
		//todo换场景的过程中被顶了
		
		//告诉账号被顶
		OnConnidEnterGame(conn, player_id);
		auto& player_session = registry.get<PlayerSession>(player);
		registry.emplace_or_replace<MsConverPlayerComp>(player);//连续顶几次,所以用emplace_or_replace
		gwservice::KickConnRequest messag;
		messag.set_session_id(player_session.gate_session_.session_id());
		Send2Gate(messag, player_session.gate_node_id());

		player_session.gate_session_.set_session_id(request->session_id());
		auto gate_id = node_id(request->session_id());
		auto gate_it = g_gate_nodes.find(gate_id);
		//断开链接必须是当前的gate去断，防止异步消息顺序,进入先到然后断开才到
		if (gate_it != g_gate_nodes.end() && player_session.gate_node_id() != gate_id)
		{
			auto gate = registry.try_get<GateNodePtr>(gate_it->second);
			if (nullptr != gate)
			{
				player_session.gate_ = *gate;
			}
		}
		auto scene = registry.get<SceneEntity>(player).scene_entity();
		auto* p_gs_data = registry.try_get<GsDataPtr>(scene);
		if (nullptr == p_gs_data)
		{
			// todo default
			LOG_INFO << "player " << player_id << " enter default secne";
		}
		auto& gs_data = *p_gs_data;
		auto it = g_gs_nodes.find(gs_data->node_id());
		if (it != g_gs_nodes.end())
		{
			gsservice::EnterGsRequest message;
            message.set_player_id(player_id);
            message.set_session_id(request->session_id());
            message.set_ms_node_id(g_ms_node->master_node_id());
            auto& scene_info = registry.get<SceneInfo>(scene);
            message.mutable_scenes_info()->CopyFrom(scene_info);
            registry.get<GsStubPtr>(it->second)->CallMethod(message, &gsservice::GsService_Stub::EnterGs);
		}
	}

///<<< END WRITING YOUR CODE 
}

void MasterNodeServiceImpl::OnLsLeaveGame(::google::protobuf::RpcController* controller,
    const msservice::LsLeaveGameRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 

	auto player_id = request->session_id();
	auto player = PlayerList::GetSingleton().GetPlayer(player_id);
	
	assert(registry.get<Guid>(player) == player_id);
	PlayerList::GetSingleton().LeaveGame(player_id);
	assert(!PlayerList::GetSingleton().HasPlayer(player_id));
	assert(PlayerList::GetSingleton().GetPlayer(player_id) == entt::null);

///<<< END WRITING YOUR CODE 
}

void MasterNodeServiceImpl::OnLsDisconnect(::google::protobuf::RpcController* controller,
    const msservice::LsDisconnectRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	auto cit = g_gate_sessions.find(request->session_id());
	if (cit == g_gate_sessions.end())
	{
		return;
	}
	auto p_try_player = registry.try_get<EntityPtr>(cit->second);
	if (nullptr == p_try_player)
	{
		return;
	}
	auto player_id = registry.get<Guid>(*p_try_player);
	PlayerList::GetSingleton().LeaveGame(player_id);
	g_gate_sessions.erase(cit);
///<<< END WRITING YOUR CODE 
}

void MasterNodeServiceImpl::OnGsPlayerService(::google::protobuf::RpcController* controller,
    const msservice::PlayerNodeServiceRequest* request,
    msservice::PlayerMessageRespone* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	auto& message_extern = request->ex();
	auto& player_msg = request->msg();
	auto it = g_players.find(message_extern.player_id());
	if (it == g_players.end())
	{
		LOG_INFO << "player not found " << message_extern.player_id();
		return;
	}
	auto msg_id = request->msg().msg_id();
	auto sit = g_serviceinfo.find(msg_id);
	if (sit == g_serviceinfo.end())
	{
		LOG_INFO << "msg not found " << msg_id;
		return;
	}
	auto service_it = g_player_services.find(sit->second.service);
	if (service_it == g_player_services.end())
	{
		LOG_INFO << "msg not found " << msg_id;
		return;
	}
	auto& serviceimpl = service_it->second;
	google::protobuf::Service* service = serviceimpl->service();
	const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
	const google::protobuf::MethodDescriptor* method = desc->FindMethodByName(sit->second.method);
	if (nullptr == method)
	{
		LOG_INFO << "msg not found " << msg_id;
		//todo client error;
		return;
	}
	std::unique_ptr<google::protobuf::Message> player_request(service->GetRequestPrototype(method).New());
	player_request->ParseFromString(player_msg.body());
	std::unique_ptr<google::protobuf::Message> player_response(service->GetResponsePrototype(method).New());
	auto player = it->second;
	serviceimpl->CallMethod(method, player, get_pointer(player_request), get_pointer(player_response));
	if (nullptr == response)//不需要回复
	{
		return;
	}
	response->mutable_ex()->set_player_id(request->ex().player_id());
	response->mutable_msg()->set_body(player_response->SerializeAsString());
	response->mutable_msg()->set_msg_id(msg_id);
///<<< END WRITING YOUR CODE 
}

void MasterNodeServiceImpl::AddCrossServerScene(::google::protobuf::RpcController* controller,
    const msservice::AddCrossServerSceneRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

void MasterNodeServiceImpl::EnterGsSucceed(::google::protobuf::RpcController* controller,
    const msservice::EnterGsSucceedRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	auto player = PlayerList::GetSingleton().GetPlayer(request->player_id());
	if (entt::null == player)
	{
		return;
	}
	auto& player_session = registry.get<PlayerSession>(player);
	auto gate_it = g_gate_nodes.find(player_session.gate_node_id());
	if (gate_it == g_gate_nodes.end())
	{
		LOG_ERROR << "gate crsh" << player_session.gate_node_id();
		return;
	}

	MasterNodeServiceImpl::Ms2GwPlayerEnterGsRpcReplied c;
	auto& message = c.s_rq_;
	message.set_session_id(player_session.session_id());
	message.set_gs_node_id(player_session.gs_node_id());
	registry.get<GwStub>(gate_it->second).CallMethodByObj(&MasterNodeServiceImpl::Ms2GwPlayerEnterGsReplied, c, this, &gwservice::GwNodeService::PlayerEnterGs);
///<<< END WRITING YOUR CODE 
}

///<<<rpc end
