#include "controller_service_handler.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/game_config/mainscene_config.h"

#include "src/comp/account_player.h"
#include "src/controller_server.h"
#include "src/game_logic/comp/scene_comp.h"
#include "src/game_logic/scene/servernode_system.h"
#include "src/comp/player_list.h"
#include "src/game_logic/comp/account_comp.h"
#include "src/game_logic/tips_id.h"
#include "src/game_logic/scene/scene_system.h"
#include "src/game_logic/thread_local/common_logic_thread_local_storage.h"
#include "src/network/gs_node.h"
#include "src/network/login_node.h"
#include "src/network/player_session.h"
#include "src/network/rpc_msg_route.h"
#include "src/pb/pbc/gate_service_service.h"
#include "src/pb/pbc/game_service_service.h"
#include "src/pb/pbc/login_service_service.h"
#include "src/pb/pbc/database_service_service.h"
#include "src/pb/pbc/service.h"
#include "src/handler/player_service.h"
#include "src/handler/register_handler.h"
#include "src/system/player_scene_system.h"
#include "src/system/player_common_system.h"
#include "src/system/player_change_scene.h"
#include "src/thread_local/controller_thread_local_storage.h"
#include "src/util/defer.h"

#include "component_proto/player_login_comp.pb.h"
#include "component_proto/player_comp.pb.h"

thread_local std::unordered_map<std::string, uint64_t> tls_login_accounts_session;

constexpr std::size_t kMaxPlayerSize{1000};

Guid GetPlayerIdByConnId(const uint64_t session_id)
{
	const auto cit = controller_tls.gate_sessions().find(session_id);
	if (cit == controller_tls.gate_sessions().end())
	{
		return kInvalidGuid;
	}
	const auto* const p_try_player = tls.registry.try_get<Guid>(cit->second);
	if (nullptr == p_try_player)
	{
		return kInvalidGuid;
	}
	return *p_try_player;
}

entt::entity GetPlayerByConnId(uint64_t session_id)
{
	const auto player_it = controller_tls.player_list().find(GetPlayerIdByConnId(session_id));
	if (player_it == controller_tls.player_list().end())
	{
		return entt::null;
	}
	return player_it->second;
}

///<<< END WRITING YOUR CODE
void ControllerServiceHandler::StartGs(::google::protobuf::RpcController* controller,
	const ::CtrlStartGsRequest* request,
	::CtrlStartGsResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	response->set_controller_node_id(controller_node_id());
	const InetAddress session_addr(request->rpc_client().ip(), request->rpc_client().port());
	const InetAddress service_addr(request->rpc_server().ip(), request->rpc_server().port());
	entt::entity game_node{entt::null};
	for (auto e : tls.registry.view<RpcServerConnection>())
	{
		if (tls.registry.get<RpcServerConnection>(e).conn_->peerAddress().toIpPort() != session_addr.toIpPort())
		{
			continue;
		}
		game_node = e;
		break;
	}
	if (game_node == entt::null)
	{
		//todo
		LOG_INFO << "game connection not found " << request->gs_node_id();
		return;
	}

	auto c = tls.registry.get<RpcServerConnection>(game_node);
	auto gs_node_ptr = std::make_shared<GsNodePtr::element_type>(c.conn_);
	gs_node_ptr->node_info_.set_node_id(request->gs_node_id());
	gs_node_ptr->node_info_.set_node_type(kGameNode);
	gs_node_ptr->node_inet_addr_ = service_addr; //为了停掉gs，或者gs断线用
	gs_node_ptr->server_entity_ = game_node;
	AddMainSceneNodeComponent(game_node);
	tls.registry.emplace<GsNodePtr>(game_node, gs_node_ptr);
	if (request->server_type() == kMainSceneServer)
	{
		const auto& config_all = mainscene_config::GetSingleton().all();
		CreateGsSceneParam create_scene_param;
		create_scene_param.node_ = game_node;
		for (int32_t i = 0; i < config_all.data_size(); ++i)
		{
			create_scene_param.scene_confid_ = config_all.data(i).id();
			auto scene_entity = ScenesSystem::CreateScene2Gs(create_scene_param);
			tls.registry.emplace<GsNodePtr>(scene_entity, gs_node_ptr);
			response->add_scenes_info()->CopyFrom(tls.registry.get<SceneInfo>(scene_entity));
		}
	}
	else if (request->server_type() == kMainSceneCrossServer)
	{
		tls.registry.remove<MainSceneServer>(game_node);
		tls.registry.emplace<CrossMainSceneServer>(game_node);
	}
	else if (request->server_type() == kRoomSceneCrossServer)
	{
		tls.registry.remove<MainSceneServer>(game_node);
		tls.registry.emplace<CrossRoomSceneServer>(game_node);
	}
	else
	{
		tls.registry.remove<MainSceneServer>(game_node);
		tls.registry.emplace<RoomSceneServer>(game_node);
	}

	for (auto gate : tls.registry.view<GateNodePtr>())
	{
		g_controller_node->LetGateConnect2Gs(game_node, gate);
	}
	controller_tls.game_node().emplace(request->gs_node_id(), game_node);
	LOG_DEBUG << "gs connect node id: " << request->gs_node_id() << response->DebugString() << "server type:" << request->server_type();
///<<< END WRITING YOUR CODE
}

void ControllerServiceHandler::GateConnect(::google::protobuf::RpcController* controller,
	const ::GateConnectRequest* request,
	::google::protobuf::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	InetAddress session_addr(request->rpc_client().ip(), request->rpc_client().port());
	entt::entity gate{ entt::null };
	for (auto e : tls.registry.view<RpcServerConnection>())
	{
		auto c = tls.registry.get<RpcServerConnection>(e);
		if (c.conn_->peerAddress().toIpPort() != session_addr.toIpPort())
		{
			continue;
		}
		gate = e;
		auto& gate_node = tls.registry.emplace<GateNodePtr>(gate, std::make_shared<GateNodePtr::element_type>(c.conn_));
		gate_node->node_info_.set_node_id(request->gate_node_id());
		gate_node->node_info_.set_node_type(kGateNode);
		gate_node->entity_id_ = e;
		controller_tls.gate_nodes().emplace(request->gate_node_id(), gate_node);
		break;
	}
	if (entt::null == gate)
	{
		return;
	}
	tls.registry.emplace<InetAddress>(gate, session_addr);
	for (auto e : tls.registry.view<GsNodePtr>())
	{
		g_controller_node->LetGateConnect2Gs(e, gate);
	}
///<<< END WRITING YOUR CODE
}

void ControllerServiceHandler::GatePlayerService(::google::protobuf::RpcController* controller,
	const ::GateClientMessageRequest* request,
	::google::protobuf::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ControllerServiceHandler::GateDisconnect(::google::protobuf::RpcController* controller,
	const ::GateDisconnectRequest* request,
	::google::protobuf::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE

	//断开链接必须是当前的gate去断，防止异步消息顺序,进入先到然后断开才到
	auto player = GetPlayerByConnId(request->session_id());
	if (entt::null == player)
	{
		return;
	}
	auto try_account = tls.registry.try_get<PlayerAccount>(player);
	if (nullptr != try_account)
	{
		tls_login_accounts_session.erase((*try_account)->account());
	}	
	auto try_player_session = tls.registry.try_get<PlayerSession>(player);
	if (nullptr == try_player_session)//玩家已经断开连接了
	{
		return;
	}
	//notice 异步过程 gate 先重连过来，然后断开才收到，也就是会把新来的连接又断了，极端情况，也要测试如果这段代码过去了，会有什么问题
	if (try_player_session->session_id() != request->session_id())
	{
		return;
	}
	auto it = controller_tls.game_node().find(try_player_session->gs_node_id());
	if (it == controller_tls.game_node().end())
	{
		return;
	}
	auto player_id = tls.registry.get<Guid>(player);
	controller_tls.gate_sessions().erase(player_id);
	GameNodeDisconnectRequest rq;
	rq.set_player_id(player_id);
	tls.registry.get<GsNodePtr>(it->second)->session_.CallMethod(GameServiceDisconnectMsgId, rq);
	ControllerPlayerSystem::LeaveGame(player_id);
///<<< END WRITING YOUR CODE
}

void ControllerServiceHandler::StartLs(::google::protobuf::RpcController* controller,
	const ::StartLsRequest* request,
	::StartLsResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	response->set_controller_node_id(controller_node_id());
	InetAddress session_addr(request->rpc_client().ip(), request->rpc_client().port());
	InetAddress service_addr(request->rpc_server().ip(), request->rpc_server().port());
	entt::entity login{ entt::null };
	for (auto e : tls.registry.view<RpcServerConnection>())
	{
		if (tls.registry.get<RpcServerConnection>(e).conn_->peerAddress().toIpPort() != session_addr.toIpPort())
		{
			continue;
		}
		login = e;
		break;
	}
	if (login == entt::null)
	{
		//todo
		LOG_INFO << "login connection not found " << request->login_node_id();
		return;
	}

	auto c = tls.registry.get<RpcServerConnection>(login);
	auto& login_node = tls.registry.emplace<LoginNode>(login, c.conn_);
	login_node.node_info_.set_node_id(request->login_node_id());
	login_node.node_info_.set_node_type(kGameNode);
	controller_tls.game_node().emplace(request->login_node_id(), login);
	LOG_DEBUG << "login connect node id: " << request->login_node_id() << response->DebugString() << "server type:" << request->server_type();
///<<< END WRITING YOUR CODE
}

void ControllerServiceHandler::LsLoginAccount(::google::protobuf::RpcController* controller,
	const ::LoginRequest* request,
	::LoginResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	auto session_it = controller_tls.gate_sessions().find(cl_tls.session_id());
	if (session_it == controller_tls.gate_sessions().end())
	{
		session_it = controller_tls.gate_sessions().emplace(cl_tls.session_id(), tls.registry.create()).first;
	}
	if (session_it == controller_tls.gate_sessions().end())
	{
		response->mutable_error()->set_id(kRetLoginUnknownError);
		return;
	}
	tls.registry.emplace<PlayerAccount>(session_it->second, std::make_shared<PlayerAccount::element_type>())->set_account(request->account());
	tls.registry.emplace<AccountLoginNode>(session_it->second, AccountLoginNode{cl_tls.session_id()});
	//todo 队列里面有同一个人的两个链接
	const auto lit = tls_login_accounts_session.find(request->account());
	if (controller_tls.player_list().size() >= kMaxPlayerSize)
	{
		//如果登录的是新账号,满了得去排队,是账号排队，还是角色排队>???
		response->mutable_error()->set_id(kRetLoginAccountPlayerFull);
		return;
	}

	if (lit != tls_login_accounts_session.end())
	{
		//如果不是同一个登录服务器,踢掉已经登录的账号
		if (lit->second != cl_tls.session_id())
		{
		}
		//告诉客户端登录中
		else
		{
			//能返回去吗?
			response->mutable_error()->set_id(kRetLoginIng);
		}
	}
	else
	{
		tls_login_accounts_session.emplace(request->account(), cl_tls.session_id());
	}
///<<< END WRITING YOUR CODE
}

void ControllerServiceHandler::LsEnterGame(::google::protobuf::RpcController* controller,
	const ::EnterGameRequest* request,
	::EnterGameResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	//todo正常或者顶号进入场景
	//todo 断线重连进入场景，断线重连分时间
	//todo 返回login session 删除了后能返回客户端吗?数据流程对吗
	const auto session_it = controller_tls.gate_sessions().find(cl_tls.session_id());
	if (session_it == controller_tls.gate_sessions().end())
	{
		LOG_ERROR << "connection not found " << cl_tls.session_id();
		return;
	}
	tls.registry.emplace<Guid>(session_it->second, request->player_id());

	const auto player_it = controller_tls.player_list().find(request->player_id());
	const auto* const try_account = tls.registry.try_get<PlayerAccount>(session_it->second);
	if (nullptr != try_account)
	{
		tls_login_accounts_session.erase((*try_account)->account());
	}
	if (player_it == controller_tls.player_list().end())
	{
		//把旧的connection 断掉
		const auto player = tls.registry.create();
		if (nullptr != try_account)
		{
			tls.registry.emplace<PlayerAccount>(player, *try_account);
		}
		tls.registry.emplace_or_replace<PlayerSession>(player).set_session_id(cl_tls.session_id());

		PlayerCommonSystem::InitPlayerComponent(player, request->player_id());

		GetSceneParam get_scene_param;
		get_scene_param.scene_conf_id_ = 1;
		const auto scene = ServerNodeSystem::GetNotFullScene(get_scene_param);
		//找不到上次的场景，放到默认场景里面
		if (scene == entt::null)
		{
			// todo default
			LOG_INFO << "player " << request->player_id() << " enter default secne";
		}
		//第一次进入
		tls.registry.emplace<EnterGsFlag>(player).set_enter_gs_type(LOGIN_FIRST);

		//todo 会话没有了玩家还在

		PlayerSceneSystem::CallPlayerEnterGs(player, PlayerSceneSystem::GetGsNodeIdByScene(scene), cl_tls.session_id());
		ControllerChangeSceneInfo change_scene_info;
		change_scene_info.mutable_scene_info()->CopyFrom(tls.registry.get<SceneInfo>(scene));
		change_scene_info.set_change_gs_type(ControllerChangeSceneInfo::eDifferentGs);
		change_scene_info.set_change_gs_status(ControllerChangeSceneInfo::eEnterGsSceneSucceed);
		PlayerChangeSceneSystem::PushChangeSceneInfo(player, change_scene_info);
	}
	else
	{
		//顶号,断线重连 记得gate 删除 踢掉老gate,但是是同一个gate就不用了
		//顶号的时候已经在场景里面了,不用再进入场景了
		//todo换场景的过程中被顶了
		const auto player = player_it->second;
		//告诉账号被顶
        //断开链接必须是当前的gate去断，防止异步消息顺序,进入先到然后断开才到
		const auto player_session = tls.registry.try_get<PlayerSession>(player);
		if (nullptr != player_session)
        {
			extern const uint32_t ClientPlayerCommonServiceBeKickMsgId;
			TipsS2C beKickTips;
			beKickTips.mutable_tips()->set_id(kRetLoginBeKickByAnOtherAccount);
			Send2Player(ClientPlayerCommonServiceBeKickMsgId, beKickTips, request->player_id());
			//删除老会话
			controller_tls.gate_sessions().erase(player_session->session_id());
			GateNodeKickConnRequest message;
            message.set_session_id(cl_tls.session_id());
            Send2Gate(GateServiceKickConnByControllerMsgId, message, player_session->gate_node_id());

			(*player_session).set_session_id(cl_tls.session_id());
        }
		else
		{
			auto& player_session = tls.registry.emplace_or_replace<PlayerSession>(player);
            player_session.set_session_id(cl_tls.session_id());
		}
		
		//连续顶几次,所以用emplace_or_replace
		tls.registry.emplace_or_replace<EnterGsFlag>(player).set_enter_gs_type(LOGIN_REPLACE);
	}
	
	
///<<< END WRITING YOUR CODE
}

void ControllerServiceHandler::LsLeaveGame(::google::protobuf::RpcController* controller,
	const ::CtrlLsLeaveGameRequest* request,
	::google::protobuf::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	ControllerPlayerSystem::LeaveGame(GetPlayerIdByConnId(cl_tls.session_id()));
	//todo statistics
///<<< END WRITING YOUR CODE
}

void ControllerServiceHandler::LsDisconnect(::google::protobuf::RpcController* controller,
	const ::CtrlLsDisconnectRequest* request,
	::google::protobuf::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	const auto player_id = GetPlayerIdByConnId(cl_tls.session_id());
	ControllerPlayerSystem::LeaveGame(player_id);
	controller_tls.gate_sessions().erase(player_id);
///<<< END WRITING YOUR CODE
}

void ControllerServiceHandler::GsPlayerService(::google::protobuf::RpcController* controller,
	const ::NodeServiceMessageRequest* request,
	::NodeServiceMessageResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	const auto session_it = controller_tls.gate_sessions().find(request->ex().session_id());
	if (session_it == controller_tls.gate_sessions().end())
	{
		LOG_ERROR << "session not found " << request->ex().session_id();
		return;
	}
	const auto try_session_player_id = tls.registry.try_get<Guid>(session_it->second);
	if (nullptr == try_session_player_id)
	{
		LOG_ERROR << "session not found " << request->ex().session_id();
		return;
	}
	const NodeMessageBody& message_extern = request->ex();
	const auto it = controller_tls.player_list().find(*try_session_player_id);
	if (it == controller_tls.player_list().end())
	{
		LOG_ERROR << "player not found " << *try_session_player_id;
		return;
	}
	if (request->msg().message_id() >= g_message_info.size())
	{
		LOG_ERROR << "message_id not found " << request->msg().message_id();
		return;
	}
	const auto& message_info = g_message_info.at(request->msg().message_id());
	const auto service_it = g_player_service.find(message_info.service);
	if (service_it == g_player_service.end())
	{
		LOG_ERROR << "player service  not found " << request->msg().message_id();
		return;
	}
	const auto& service_handler = service_it->second;
	google::protobuf::Service* service = service_handler->service();
	const google::protobuf::MethodDescriptor* method = service->GetDescriptor()->FindMethodByName(message_info.method);
	if (nullptr == method)
	{
		LOG_ERROR << "method not found " << request->msg().message_id();
		//todo client error;
		return;
	}
	const MessagePtr player_request(service->GetRequestPrototype(method).New());
	if (!player_request->ParsePartialFromArray(request->msg().body().data(), int32_t(request->msg().body().size())))
	{
        LOG_ERROR << "ParsePartialFromArray " << request->msg().message_id();
        //todo client error;
        return;
	}
	const MessagePtr player_response(service->GetResponsePrototype(method).New());
	service_handler->CallMethod(method, it->second, get_pointer(player_request), get_pointer(player_response));
	if (nullptr == response)//不需要回复
	{
		return;
	}
	response->mutable_ex()->set_session_id(request->ex().session_id());
    auto byte_size = int32_t(response->ByteSizeLong());
	response->mutable_msg()->mutable_body()->resize(byte_size);
    // FIXME: error check
    if (response->SerializePartialToArray(response->mutable_msg()->mutable_body()->data(), byte_size))
    {
        LOG_ERROR << "message error " << this;
        return;
    }
	response->mutable_msg()->set_message_id(request->msg().message_id());
///<<< END WRITING YOUR CODE
}

void ControllerServiceHandler::AddCrossServerScene(::google::protobuf::RpcController* controller,
	const ::AddCrossServerSceneRequest* request,
	::google::protobuf::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	CreateGsSceneParam create_scene_param;
	for (auto& it : request->cross_scenes_info())
	{
		auto git = controller_tls.game_node().find(it.gs_node_id());
		if (git == controller_tls.game_node().end())
		{
			continue;
		}
		auto gs = git->second;
		auto try_gs_node_ptr = tls.registry.try_get<GsNodePtr>(gs);
		if (nullptr == try_gs_node_ptr)
		{
            LOG_ERROR << "gs not found ";
            continue;
		}
		create_scene_param.scene_info = it.scene_info();
		auto scene = ScenesSystem::CreateScene2Gs(create_scene_param);
		tls.registry.emplace<GsNodePtr>(scene, *try_gs_node_ptr);
	}
///<<< END WRITING YOUR CODE
}

void ControllerServiceHandler::EnterGsSucceed(::google::protobuf::RpcController* controller,
	const ::EnterGsSucceedRequest* request,
	::google::protobuf::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	auto player = ControllerPlayerSystem::GetPlayer(request->player_id());
	if (entt::null == player)
	{
		return;
	}
	auto& player_session = tls.registry.get<PlayerSession>(player);
	auto gate_it = controller_tls.gate_nodes().find(player_session.gate_node_id());
	if (gate_it == controller_tls.gate_nodes().end())
	{
		LOG_ERROR << "gate crash" << player_session.gate_node_id();
		return;
	}
	
	auto game_it = controller_tls.game_node().find(request->game_node_id());
	if (game_it == controller_tls.game_node().end())
	{
        LOG_ERROR << "game crash" << request->game_node_id();
        return;
	}
	auto try_gs = tls.registry.try_get<GsNodePtr>(game_it->second);
	if (nullptr == try_gs)
	{
		LOG_ERROR << "game crash" << request->game_node_id();
		return;
	}
	player_session.set_gs(*try_gs);
	GateNodePlayerEnterGsRequest rq;
	rq.set_session_id(player_session.session_id());
	rq.set_gs_node_id(player_session.gs_node_id());
	gate_it->second->session_.CallMethod(GateServicePlayerEnterGsMsgId, rq);
	PlayerChangeSceneSystem::SetChangeGsStatus(player, ControllerChangeSceneInfo::eEnterGsSceneSucceed);
	PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
///<<< END WRITING YOUR CODE
}

void ControllerServiceHandler::RouteNodeStringMsg(::google::protobuf::RpcController* controller,
	const ::RouteMsgStringRequest* request,
	::RouteMsgStringResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE

    //函数返回前一定会执行的函数
    defer(cl_tls.set_next_route_node_type(UINT32_MAX));
    defer(cl_tls.set_next_route_node_id(UINT32_MAX));
	defer(cl_tls.set_current_session_id(kInvalidSessionId));

    cl_tls.set_current_session_id(request->session_id());

if (request->route_data_list_size() >= kMaxRouteSize)
{
	LOG_ERROR << "route msg size too max:" << request->DebugString();
	return;
}
if (request->route_data_list().empty())
{
	LOG_ERROR << "msg list empty:" << request->DebugString();
	return;
}
auto& route_data = request->route_data_list(request->route_data_list_size() - 1);
if (route_data.message_id() >= g_message_info.size())
{
	LOG_INFO << "message_id not found " << route_data.message_id();
	return;
}
const auto& message_info = g_message_info[route_data.message_id()];
if (nullptr == message_info.service_impl_instance_)
{
	LOG_INFO << "message_id not found " << route_data.message_id();
	return;
}

const auto it = g_server_service.find(message_info.service);
if (it == g_server_service.end())
{
	LOG_INFO << "message_id not found " << route_data.message_id();
	return;
}
const auto& servcie = it->second;
const google::protobuf::MethodDescriptor* method = servcie->GetDescriptor()->FindMethodByName(message_info.method);
if (nullptr == method)
{
	LOG_ERROR << "method not found" << request->DebugString();
	return;
}
//当前节点的请求信息
std::unique_ptr<google::protobuf::Message> current_node_request(GetRequestPrototype(method).New());
if (!current_node_request->ParsePartialFromArray(request->body().data(), int32_t(request->body().size())))
{
	LOG_ERROR << "invalid  body request" << request->DebugString();
	return;
}

//当前节点的真正回复的消息
std::unique_ptr<google::protobuf::Message> current_node_response(GetResponsePrototype(method).New());
servcie->CallMethod(method, NULL, get_pointer(current_node_request), get_pointer(current_node_response), nullptr);

auto* mutable_request = const_cast<::RouteMsgStringRequest*>(request);
//没有发送到下个节点就是要回复了
if (cl_tls.next_route_node_type() == UINT32_MAX)
{
	auto byte_size = int32_t(current_node_response->ByteSizeLong());
	response->mutable_body()->resize(byte_size);
	current_node_response->SerializePartialToArray(response->mutable_body()->data(), byte_size);
	for (auto& request_data_it : request->route_data_list())
	{
		*response->add_route_data_list() = request_data_it;
	}
	response->set_session_id(cl_tls.session_id());
	response->set_id(request->id());
	return;
}
//处理,如果需要继续路由则拿到当前节点信息
//需要发送到下个节点
const auto next_route_data = mutable_request->add_route_data_list();
next_route_data->CopyFrom(cl_tls.route_data());
next_route_data->mutable_node_info()->CopyFrom(g_controller_node->node_info());
mutable_request->set_body(cl_tls.route_msg_body());
    mutable_request->set_id(request->id());

    switch (cl_tls.next_route_node_type())
    {
    case kLoginNode:
	{
		auto login_node_it = controller_tls.login_node().find(cl_tls.next_route_node_id());
		if (login_node_it == controller_tls.login_node().end())
		{
			LOG_ERROR << "login not found node id "  << cl_tls.next_route_node_id() << request->DebugString();
			return;
		}
		auto try_login = tls.registry.try_get<LoginNode>(login_node_it->second);
		if (nullptr == try_login)
		{
			LOG_ERROR << "login not found node id " << cl_tls.next_route_node_id() << request->DebugString();
			return;
		}
		(*try_login).session_.Route2Node(LoginServiceRouteNodeStringMsgMsgId, *mutable_request);
	}
	    break;
    case kDatabaseNode:
	{
		g_controller_node->database_node()->Route2Node(DbServiceRouteNodeStringMsgMsgId, *mutable_request);
	}
	    break;
    case kGateNode:
	{
		auto gate_it = controller_tls.gate_nodes().find(cl_tls.next_route_node_id());
		if (gate_it == controller_tls.gate_nodes().end())
		{
			LOG_ERROR << "gate not found node id " << cl_tls.next_route_node_id() << request->DebugString();
			return;
		}
		gate_it->second->session_.Route2Node(GateServiceRouteNodeStringMsgMsgId, *mutable_request);
	}
	break;
    case kGameNode:
	{
		auto controller_it = controller_tls.game_node().find(cl_tls.next_route_node_id());
		if (controller_it == controller_tls.game_node().end())
		{
			LOG_ERROR << "game not found game " << cl_tls.next_route_node_id() << request->DebugString();
			return;
		}
		auto try_game = tls.registry.try_get<GsNodePtr>(controller_it->second);
		if (nullptr == try_game)
		{
			LOG_ERROR << "game not found game " << cl_tls.next_route_node_id() << request->DebugString();
			return;
		}
		(*try_game)->session_.Route2Node(GameServiceRouteNodeStringMsgMsgId, *mutable_request);
	}
	    break;
    default:
	    {
		    LOG_ERROR << "route to next node type error " << request->DebugString() << "," << cl_tls.next_route_node_type();
	    }
	break;
}

///<<< END WRITING YOUR CODE
}

void ControllerServiceHandler::RoutePlayerStringMsg(::google::protobuf::RpcController* controller,
	const ::RoutePlayerMsgStringRequest* request,
	::RoutePlayerMsgStringResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

