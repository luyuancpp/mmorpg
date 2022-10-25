#include "ms_service.h"
#include "src/network/rpc_closure.h"
///<<< BEGIN WRITING YOUR CODE
/// #include "muduo/base/Logging.h"
#include "muduo/net/InetAddress.h"

#include "src/game_config/mainscene_config.h"

#include "src/network/gate_node.h"
#include "src/network/gs_node.h"
#include "src/network/player_session.h"
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

#include "component_proto/player_comp.pb.h"
#include "component_proto/player_login_comp.pb.h"
#include "gs_service.pb.h"
#include "logic_proto/common_server_player.pb.h"
#include "logic_proto/scene_normal.pb.h"

using GsStubPtr = std::unique_ptr<RpcStub<gsservice::GsService_Stub>>;
using GwStub = RpcStub<gwservice::GwNodeService_Stub>;

std::size_t kMaxPlayerSize = 1000;

void MasterNodeServiceImpl::Ms2GwPlayerEnterGsReplied(Ms2GwPlayerEnterGsRpc replied)
{
	//todo 中间返回是断开了
	entt::entity player = GetPlayerByConnId(replied.s_rq_.session_id());
	if (entt::null == player)
	{
		LOG_ERROR << "player not found " << registry.get<Guid>(player);
		return;
	
	}
	UpdateGateSessionGsRequest message;
    auto try_player_session = registry.try_get<PlayerSession>(player);
    if (nullptr == try_player_session)
    {
        LOG_ERROR << "player session not valid" << registry.try_get<Guid>(player);
        return;
    }
    message.set_session_id(try_player_session->session_id());
    Send2GsPlayer(message, player);
	auto try_enter_gs = registry.try_get<EnterGsFlag>(player);
	if (nullptr != try_enter_gs)
	{
		auto enter_gs_type = try_enter_gs->enter_gs_type();
		if (enter_gs_type != LOGIN_NONE)
		{
			PlayerCommonSystem::OnLogin(player);
		}
	}
	//如果进入场景的时候断线重连呢？
	if (nullptr == try_enter_gs ||//正常进入gs换场景
		try_enter_gs->enter_gs_type() == LOGIN_FIRST)//第一次登录(非顶号，重连则）调用进入场景接口
    {
        PlayerSceneSystem::OnEnterScene(player);
	}		
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

void MasterNodeServiceImpl::OnSessionEnterGame(entt::entity conn, Guid player_id)
{
    registry.emplace<EntityPtr>(conn, g_player_list->GetPlayerPtr(player_id));
    registry.emplace<Guid>(conn, player_id);
}

void MasterNodeServiceImpl::InitPlayerSession(entt::entity player, uint64_t session_id)
{
    auto& player_session = registry.get_or_emplace<PlayerSession>(player);
    player_session.gate_session_.set_session_id(session_id);
    auto gate_it = g_gate_nodes.find(node_id(session_id));
    if (gate_it == g_gate_nodes.end())
    {
		return;  
    }
    auto gate = registry.try_get<GateNodePtr>(gate_it->second);
    if (nullptr == gate)
    {
		return;        
    }
	auto try_scene_entity = registry.try_get<SceneEntity>(player);
	if (nullptr == try_scene_entity)
	{
		LOG_ERROR << "player scene empty" << registry.get<Guid>(player);
		return;
	}

    auto* p_gs_data = registry.try_get<GsNodePtr>(try_scene_entity->scene_entity_);
    if (nullptr == p_gs_data)//找不到gs了，放到好的gs里面
    {
        // todo default
		LOG_ERROR << "player " << registry.get<Guid>(player) << " enter default secne";
    }
	else
	{
		registry.get<PlayerSession>(player).gs_ = *p_gs_data;
	}
	
	player_session.gate_ = *gate;
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
	response->set_master_node_id(master_node_id());
	InetAddress session_addr(request->rpc_client().ip(), request->rpc_client().port());
	InetAddress service_addr(request->rpc_server().ip(), request->rpc_server().port());
	entt::entity gs{ entt::null };
	for (auto e : registry.view<RpcServerConnection>())
	{
		if (registry.get<RpcServerConnection>(e).conn_->peerAddress().toIpPort() != session_addr.toIpPort())
		{
			continue;
		}
		gs = e;
		break;
	}
	if (gs == entt::null)
	{
		//todo
		LOG_INFO << "game connection not found " << request->gs_node_id();
		return;
	}

	auto c = registry.get<RpcServerConnection>(gs);
	GsNodePtr gs_node_ptr = std::make_shared<GsNodePtr::element_type>(c.conn_);
	gs_node_ptr->node_info_.set_node_id(request->gs_node_id());
	gs_node_ptr->node_info_.set_node_type(kGsNode);
	AddMainSceneNodeCompnent(gs);
	registry.emplace<InetAddress>(gs, service_addr);//为了停掉gs，或者gs断线用
	registry.emplace<GsNodePtr>(gs, gs_node_ptr);
	registry.emplace<GsServer>(gs);
	registry.emplace<GsStubPtr>(gs, std::make_unique<GsStubPtr::element_type>(boost::any_cast<muduo::net::RpcChannelPtr>(c.conn_->getContext())));	
	if (request->server_type() == kMainSceneServer)
	{
		auto& config_all = mainscene_config::GetSingleton().all();
		CreateGsSceneP create_scene_param;
		create_scene_param.node_ = gs;
		for (int32_t i = 0; i < config_all.data_size(); ++i)
		{
			create_scene_param.scene_confid_ = config_all.data(i).id();
			auto scene_entity = ScenesSystem::CreateScene2Gs(create_scene_param);
			registry.emplace<GsNodePtr>(scene_entity, gs_node_ptr);
			response->add_scenes_info()->CopyFrom(registry.get<SceneInfo>(scene_entity));
		}
	}
	else if (request->server_type() == kMainSceneCrossServer)
	{
        registry.remove<MainSceneServer>(gs);
        registry.emplace<CrossMainSceneServer>(gs);
	}
    else if (request->server_type() == kRoomSceneCrossServer)
    {
        registry.remove<MainSceneServer>(gs);
        registry.emplace<CrossRoomSceneServer>(gs);
    }
	else
	{
		registry.remove<MainSceneServer>(gs);
		registry.emplace<RoomSceneServer>(gs);
	}

	for (auto e : registry.view<GateNodePtr>())
	{
		g_ms_node->LetGateConnect2Gs(gs, e);
	}
	g_gs_nodes.emplace(registry.get<GsNodePtr>(gs)->node_info_.node_id(), gs);
	LOG_INFO << "game connected " << request->gs_node_id() << response->DebugString();
///<<< END WRITING YOUR CODE 
}

void MasterNodeServiceImpl::OnGwConnect(::google::protobuf::RpcController* controller,
    const msservice::ConnectRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	InetAddress session_addr(request->rpc_client().ip(), request->rpc_client().port());
	entt::entity gate{ entt::null };
	for (auto e : registry.view<RpcServerConnection>())
	{
		auto c = registry.get<RpcServerConnection>(e);
		auto& local_addr = c.conn_->peerAddress();
		if (local_addr.toIpPort() != session_addr.toIpPort())
		{
			continue;
		}
		gate = e;
		auto& gate_node = *registry.emplace<GateNodePtr>(gate, std::make_shared<GateNode>(c.conn_));
		gate_node.node_info_.set_node_id(request->gate_node_id());
		gate_node.node_info_.set_node_type(kGatewayNode);
		g_gate_nodes.emplace(request->gate_node_id(), gate);
        registry.emplace_or_replace<GwStub>(gate, GwStub(boost::any_cast<muduo::net::RpcChannelPtr>(c.conn_->getContext())));
		break;
	}
	registry.emplace<InetAddress>(gate, session_addr);
	for (auto e : registry.view<GsServer>())
	{
		g_ms_node->LetGateConnect2Gs(e, gate);
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

	//断开链接必须是当前的gate去断，防止异步消息顺序,进入先到然后断开才到
	auto player = GetPlayerByConnId(request->session_id());
	if (entt::null == player)
	{
		return;
	}
	auto try_acount = registry.try_get<PlayerAccount>(player);
	if (nullptr != try_acount)
	{
		logined_accounts_sesion_.erase(**try_acount);
	}	
	auto try_player_session = registry.try_get<PlayerSession>(player);
	if (nullptr == try_player_session)//玩家已经断开连接了
	{
		return;
	}
	//notice 异步过程 gate 先重连过来，然后断开才收到，也就是会把新来的连接又断了，极端情况，也要测试如果这段代码过去了，会有什么问题
	if (try_player_session->session_id() != request->session_id())
	{
		return;
	}
	auto it = g_gs_nodes.find(try_player_session->gs_node_id());
	if (it == g_gs_nodes.end())
	{
		return;
	}
	auto player_id = registry.get<Guid>(player);
	g_gate_sessions.erase(player_id);
	gsservice::DisconnectRequest message;
	message.set_player_id(player_id);
	registry.get<GsStubPtr>(it->second)->CallMethod(
		message,
		&gsservice::GsService_Stub::Disconnect);
	g_player_list->LeaveGame(player_id);
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
    registry.emplace<PlayerAccount>(conn, std::make_shared<PlayerAccount::element_type>(request->account()));
    registry.emplace<AccountLoginNode>(conn, AccountLoginNode{request->session_id()});
	//todo 
	auto lit = logined_accounts_sesion_.find(request->account());
	if (g_player_list->player_size() >= kMaxPlayerSize)
	{
		//如果登录的是新账号,满了得去排队,是账号排队，还是角色排队>???
		response->mutable_error()->set_id(kRetLoginAccountPlayerFull);
		return;
	}

	if (lit != logined_accounts_sesion_.end())
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
		logined_accounts_sesion_.emplace(request->account(), request->session_id());
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
    auto sit = g_gate_sessions.find(request->session_id());
	if (sit == g_gate_sessions.end())
	{
		LOG_ERROR << "connection not found " << request->session_id();
		return;
	}
	auto session = sit->second;
	auto player_id = request->player_id();
	auto player = g_player_list->GetPlayer(player_id);
	auto try_acount = registry.try_get<PlayerAccount>(session);
	if (nullptr != try_acount)
	{
		logined_accounts_sesion_.erase(**try_acount);
	}
	if (entt::null == player)
	{
		//把旧的connection 断掉
		player = g_player_list->EnterGame(player_id);
		OnSessionEnterGame(session, player_id);
		registry.emplace<Guid>(player, player_id);
		registry.emplace<Player>(player);
		registry.emplace<PlayerAccount>(player, registry.get<PlayerAccount>(sit->second));
		
		GetSceneParam getp;
		getp.scene_confid_ = 1;
		auto scene = ServerNodeSystem::GetMainSceneNotFull(getp);
		if (scene == entt::null)//找不到上次的场景，放到默认场景里面
		{
			// todo default
			LOG_INFO << "player " << player_id << " enter default secne";
		} 

        EnterSceneParam ep;
        ep.enterer_ = player;
        ep.scene_ = scene;
        ScenesSystem::EnterScene(ep);//顶号的时候已经在场景里面了
		InitPlayerSession(player, request->session_id());
		registry.emplace<EnterGsFlag>(player).set_enter_gs_type(LOGIN_FIRST);

	}
	else//顶号,断线重连 记得gate 删除 踢掉老gate,但是是同一个gate就不用了
	{
		//todo换场景的过程中被顶了
		
		//告诉账号被顶
		OnSessionEnterGame(session, player_id);
        //断开链接必须是当前的gate去断，防止异步消息顺序,进入先到然后断开才到
        auto player_session = registry.try_get<PlayerSession>(player);
        if (nullptr != player_session)
        {
            gwservice::KickConnRequest message;
            message.set_session_id(player_session->gate_session_.session_id());
            Send2Gate(message, player_session->gate_node_id());
        }
		InitPlayerSession(player, request->session_id());
		registry.emplace_or_replace<EnterGsFlag>(player).set_enter_gs_type(LOGIN_REPLACE);//连续顶几次,所以用emplace_or_replace
	}
	if (entt::null == player)
	{
		LOG_ERROR << "player enter game";
		return;
	}
	PlayerSceneSystem::SendEnterGs(player);
///<<< END WRITING YOUR CODE 
}

void MasterNodeServiceImpl::OnLsLeaveGame(::google::protobuf::RpcController* controller,
    const msservice::LsLeaveGameRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 

	auto player_id = GetPlayerIdByConnId(request->session_id());
	g_player_list->LeaveGame(player_id);
	//todo statistics
///<<< END WRITING YOUR CODE 
}

void MasterNodeServiceImpl::OnLsDisconnect(::google::protobuf::RpcController* controller,
    const msservice::LsDisconnectRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	auto player_id = GetPlayerIdByConnId(request->session_id());
	g_player_list->LeaveGame(player_id);
	g_gate_sessions.erase(player_id);
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
	auto it = g_players.find(message_extern.player_id());
	if (it == g_players.end())
	{
		LOG_INFO << "player not found " << message_extern.player_id();
		return;
	}
	auto& message = request->msg();
	auto message_id = message.msg_id();
	auto sit = g_serviceinfo.find(message_id);
	if (sit == g_serviceinfo.end())
	{
		LOG_INFO << "msg not found " << message_id;
		return;
	}
	auto service_it = g_player_services.find(sit->second.service);
	if (service_it == g_player_services.end())
	{
		LOG_INFO << "msg not found " << message_id;
		return;
	}
	auto& serviceimpl = service_it->second;
	google::protobuf::Service* service = serviceimpl->service();
	const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
	const google::protobuf::MethodDescriptor* method = desc->FindMethodByName(sit->second.method);
	if (nullptr == method)
	{
		LOG_INFO << "message not found " << message_id;
		//todo client error;
		return;
	}
	std::unique_ptr<google::protobuf::Message> player_request(service->GetRequestPrototype(method).New());
	player_request->ParseFromString(message.body());
	std::unique_ptr<google::protobuf::Message> player_response(service->GetResponsePrototype(method).New());
	serviceimpl->CallMethod(method, it->second, get_pointer(player_request), get_pointer(player_response));
	if (nullptr == response)//不需要回复
	{
		return;
	}
	response->mutable_ex()->set_player_id(request->ex().player_id());
	response->mutable_msg()->set_body(player_response->SerializeAsString());
	response->mutable_msg()->set_msg_id(message_id);
///<<< END WRITING YOUR CODE 
}

void MasterNodeServiceImpl::AddCrossServerScene(::google::protobuf::RpcController* controller,
    const msservice::AddCrossServerSceneRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
    CreateSceneBySceneInfoP create_scene_param;
	for (auto& it : request->cross_scenes_info())
	{
		auto git = g_gs_nodes.find(it.gs_node_id());
		if (git == g_gs_nodes.end())
		{
			continue;
		}
		auto gs = git->second;
		auto try_gs_node_ptr = registry.try_get<GsNodePtr>(gs);
		if (nullptr == try_gs_node_ptr)
		{
            LOG_ERROR << "gs not found ";
            continue;
		}
		create_scene_param.scene_info_ = it.scene_info();
        auto scene = ScenesSystem::CreateSceneByGuid(create_scene_param);
		registry.emplace<GsNodePtr>(scene, *try_gs_node_ptr);
	}
///<<< END WRITING YOUR CODE 
}

void MasterNodeServiceImpl::EnterGsSucceed(::google::protobuf::RpcController* controller,
    const msservice::EnterGsSucceedRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	auto player = g_player_list->GetPlayer(request->player_id());
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
	MasterNodeServiceImpl::Ms2GwPlayerEnterGsRpc rpc;
	rpc.s_rq_.set_session_id(player_session.session_id());
	rpc.s_rq_.set_gs_node_id(player_session.gs_node_id());
	registry.get<GwStub>(gate_it->second).CallMethodByObj(&MasterNodeServiceImpl::Ms2GwPlayerEnterGsReplied, rpc, this, &gwservice::GwNodeService::PlayerEnterGs);
///<<< END WRITING YOUR CODE 
}

///<<<rpc end
