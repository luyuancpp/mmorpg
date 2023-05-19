#include "login_service.h"
#include "src/network/rpc_msg_route.h"
///<<< BEGIN WRITING YOUR CODE  
#include "muduo/base/Logging.h"

#include "src/util/game_registry.h"
#include "src/network/rpc_server.h"
#include "src/game_logic/thread_local/common_logic_thread_local_storage.h"
#include "src/game_logic/tips_id.h"
#include "src/login_server.h"
#include "src/comp/account_player.h"
#include "src/network/rpc_msg_route.h"
#include "src/network/rpc_client.h"
#include "src/network/route_system.h"
#include "src/network/node_info.h"
#include "src/redis_client/redis_client.h"
#include "src/pb/pbc/controller_service_service.h"
#include "src/pb/pbc/database_service_service.h"
#include "src/pb/pbc/service.h"

#include "login_service.pb.h"
#include "database_service.pb.h"
#include "controller_service.pb.h"

using namespace muduo;
using namespace muduo::net;

using PlayerPtr = std::shared_ptr<AccountPlayer>;
using LoginPlayersMap = std::unordered_map<std::string, PlayerPtr>;
using ConnectionEntityMap = std::unordered_map<Guid, PlayerPtr>;

ConnectionEntityMap sessions_;

using EnterGameControllerRpc = std::shared_ptr<RpcString<CtrlEnterGameRequest, CtrlEnterGameResponese, LoginNodeEnterGameResponse>>;
void EnterGameReplied(EnterGameControllerRpc replied)
{
	sessions_.erase(replied->s_rq_.session_id());
}

void EnterGame(Guid player_id,
	uint64_t session_id,
	LoginNodeEnterGameResponse* response,
	::google::protobuf::Closure* done)
{
	auto it = sessions_.find(session_id);
	if (sessions_.end() == it)
	{
		ReturnClosureError(kRetLoginEnterGameConnectionAccountEmpty);
	}
	auto rpc(std::make_shared<EnterGameControllerRpc::element_type>(response, done));
	rpc->s_rq_.set_player_id(player_id);
	rpc->s_rq_.set_session_id(response->session_id());/*
	g_login_node->controller_node().CallMethodString1(
		EnterGameReplied,
		rpc,
		&ControllerService::ControllerNodeService_Stub::OnLsEnterGame);*/

}

void UpdateAccount(uint64_t session_id, const ::account_database& a_d)
{
	auto sit = sessions_.find(session_id);
	if (sit == sessions_.end())//断线了
	{
		return;
	}
	sit->second->set_account_data(a_d);
	sit->second->OnDbLoaded();
}

using LoginAccountDbRpc = std::shared_ptr< RpcString<DatabaseNodeLoginRequest, DatabaseNodeLoginResponse, LoginNodeLoginResponse>>;
void LoginAccountDbReplied(LoginAccountDbRpc replied)
{
	auto& srp = replied->s_rp_;
	replied->c_rp_->mutable_account_player()->CopyFrom(srp->account_player());
	UpdateAccount(replied->s_rq_.session_id(), srp->account_player());
}

using LoginAcountControllerRpc = std::shared_ptr<RpcString<CtrlLoginAccountRequest, CtrlLoginAccountResponse, LoginNodeLoginResponse>>;
void LoginAccountControllerReplied(LoginAcountControllerRpc replied)
{
	//只连接不登录,占用连接
	// login process
	// check account rule: empty , errno
	// check string rule
	auto sit = sessions_.find(replied->s_rq_.session_id());
	if (sit == sessions_.end())
	{
		replied->c_rp_->mutable_error()->set_id(kRetLoginCreatePlayerConnectionHasNotAccount);
		return;
	}
	//has data
	{
		auto& player = sit->second;
		auto ret = player->Login();
		if (ret != kRetOK)
		{
			replied->c_rp_->mutable_error()->set_id(ret);
			return;
		}
		g_login_node->redis_client()->Load(player->account_data(), replied->s_rq_.account());
		if (!player->account_data().password().empty())
		{
			replied->c_rp_->mutable_account_player()->CopyFrom(player->account_data());
			player->OnDbLoaded();
			return;
		}
	}
	// database process
	auto rpc(std::make_shared<LoginAccountDbRpc::element_type>(*replied));
	rpc->s_rq_.set_account(replied->s_rq_.account());
	rpc->s_rq_.set_session_id(replied->s_rq_.session_id());
	//g_login_node->db_node().CallMethodString1(LoginAccountDbReplied, rpc, &dbservice::DbService_Stub::Login);
}

using CreatePlayerRpc = std::shared_ptr<RpcString<DatabaseNodeCreatePlayerRequest, DatabaseNodeCreatePlayerResponse, LoginNodeCreatePlayerResponse>>;
void CreatePlayerDbReplied(CreatePlayerRpc replied)
{
	auto& srp = replied->s_rp_;
	replied->c_rp_->mutable_account_player()->CopyFrom(srp->account_player());
	UpdateAccount(replied->s_rq_.session_id(), srp->account_player());
}

using EnterGameDbRpc = std::shared_ptr<RpcString<DatabaseNodeEnterGameRequest, DatabaseNodeEnterGameResponse, LoginNodeEnterGameResponse>>;
void EnterGameDbReplied(EnterGameDbRpc replied)
{
	//db 加载过程中断线了
	auto& srq = replied->s_rq_;
	auto sit = sessions_.find(replied->c_rp_->session_id());
	if (sit == sessions_.end())
	{
		return;
	}
	LoginNodeEnterGameResponse* response = nullptr;
	::google::protobuf::Closure* done = nullptr;
	replied->Move(response, done);
	EnterGame(srq.player_id(), response->session_id(), response, done);
}



///<<< END WRITING YOUR CODE

///<<<rpc begin
void LoginServiceImpl::Login(::google::protobuf::RpcController* controller,
    const ::LoginRequest* request,
    ::LoginResponse* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE

	//测试用例连接不登录马上断线，
	//账号登录马上在redis 里面，考虑第一天注册很多账号的时候账号内存很多，何时回收
	//登录的时候马上断开连接换了个gate应该可以登录成功
	//login controller
	CtrlLoginAccountRequest rq;
	rq.set_account(request->account());
	uint64_t session_id = 1;
	sessions_.emplace(session_id, std::make_shared<PlayerPtr::element_type>());
	Route2Node(kControllerNode, rq, ControllerServiceLsLoginAccountMethod);
	//LoginAccountControllerReplied
///<<< END WRITING YOUR CODE
}

void LoginServiceImpl::CreatPlayer(::google::protobuf::RpcController* controller,
    const ::CreatePlayerC2lRequest* request,
    ::LoginNodeCreatePlayerResponse* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	// login process
	//check name rule
	auto sit = sessions_.find(request->session_id());
	if (sit == sessions_.end())
	{
		ReturnClosureError(kRetLoginCreatePlayerConnectionHasNotAccount);
	}
	CheckReturnClosureError(sit->second->CreatePlayer());
	// database process
	DatabaseNodeCreatePlayerRequest rq;
	rq.set_session_id(request->session_id());
	rq.set_account(sit->second->account());
	/*g_login_node->db_node().CallMethodString1(
		CreatePlayerDbReplied,
		rpc,
		&dbservice::DbService_Stub::CreatePlayer);*/
///<<< END WRITING YOUR CODE
}

void LoginServiceImpl::EnterGame(::google::protobuf::RpcController* controller,
    const ::EnterGameC2LRequest* request,
    ::LoginNodeEnterGameResponse* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE

	auto session_id = request->session_id();
	auto sit = sessions_.find(session_id);
	if (sit == sessions_.end())
	{
		ReturnClosureError(kRetLoginEnterGameConnectionAccountEmpty);
	}
	// check second times change player id error 
	CheckReturnClosureError(sit->second->EnterGame());

	// long time in login processing
	auto player_id = request->player_id();
	if (!sit->second->HasPlayer(player_id))
	{
		ReturnClosureError(kRetLoginPlayerGuidError);
	}
	//todo 已经在其他login
	player_database new_player;
	g_login_node->redis_client()->Load(new_player, player_id);
	sit->second->Playing(player_id);//test
	response->set_session_id(session_id);
	response->set_player_id(player_id);//test
	if (new_player.player_id() > 0)
	{
		//玩家数据已经在redis里面了直接进入游戏
		::EnterGame(player_id, session_id, response, done);
		return;
	}
	// redis没有玩家数据去数据库取
	auto c(std::make_shared<EnterGameDbRpc::element_type>(response, done));
	auto& srq = c->s_rq_;
	srq.set_player_id(player_id);
	/*g_login_node->db_node().CallMethodString1(
		EnterGameDbReplied,
		c,
		&dbservice::DbService_Stub::EnterGame);*/
///<<< END WRITING YOUR CODE
}

void LoginServiceImpl::LeaveGame(::google::protobuf::RpcController* controller,
    const ::LeaveGameC2LRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	auto sit = sessions_.find(request->session_id());
	if (sit == sessions_.end())
	{
		LOG_ERROR << " leave game not found connection";
		return;
	}
	//连接过，登录过
	CtrlLsLeaveGameRequest rq;
	rq.set_session_id(request->session_id());
	g_login_node->controller_node()->CallMethod(ControllerServiceLsLeaveGameMethod, &rq);
	sessions_.erase(sit);
///<<< END WRITING YOUR CODE
}

void LoginServiceImpl::Disconnect(::google::protobuf::RpcController* controller,
    const ::LoginNodeDisconnectRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	//比如:登录还没到controller,gw的disconnect 先到，登录后到，那么controller server 永远删除不了这个sessionid了
	sessions_.erase(request->session_id());
	CtrlLsDisconnectRequest rq;
	rq.set_session_id(request->session_id());
	g_login_node->controller_node()->CallMethod(ControllerServiceLsDisconnectMethod, &rq);
///<<< END WRITING YOUR CODE
}

void LoginServiceImpl::RouteNodeStringMsg(::google::protobuf::RpcController* controller,
    const ::RouteMsgStringRequest* request,
    ::RouteMsgStringResponse* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	
	if (request->route_data_list_size() >= kMaxRouteSize)
	{
		LOG_ERROR << "route msg size too max:" << request->DebugString();
		return;
	}
	else if (request->route_data_list_size() <= 0)
	{
		LOG_ERROR << "msg list empty:" << request->DebugString();
		return;
	}
	//当前节点收到的数据
	auto& next_route_data = request->route_data_list(request->route_data_list_size() - 1);
	auto sit = g_service_method_info.find(next_route_data.service_method_id());
	if (sit == g_service_method_info.end())
	{
		LOG_INFO << "service_method_id not found " << next_route_data.service_method_id();
		return;
	}
	const google::protobuf::MethodDescriptor* method = GetDescriptor()->FindMethodByName(sit->second.method);
	if (nullptr == method)
	{
		LOG_ERROR << "method not found" << request->DebugString() << "method name" << next_route_data.method();
		return;
	}
	//当前节点的请求信息
	std::unique_ptr<google::protobuf::Message> current_node_request(GetRequestPrototype(method).New());
	if (!current_node_request->ParseFromString(request->body()))
	{
		LOG_ERROR << "invalid  body request" << request->DebugString() << "method name" << next_route_data.method();
		return;
	}
	//当前节点的真正回复的消息
	std::unique_ptr<google::protobuf::Message> current_node_response(GetResponsePrototype(method).New());
	CallMethod(method, NULL, get_pointer(current_node_request), get_pointer(current_node_response), nullptr);
	auto mutable_request = const_cast<::RouteMsgStringRequest*>(request);
	//没有发送到下个节点就是要回复了
	if (cl_tls.next_route_node_type() == UINT32_MAX)
	{ 
		response->set_body(current_node_response->SerializeAsString());
		for (auto& it : request->route_data_list())
		{
			*response->add_route_data_list() = it;
		}
		response->set_session_id(request->session_id());
		return;
	}
	//处理,如果需要继续路由则拿到当前节点信息
	//需要发送到下个节点
    cl_tls.set_next_route_node_type(UINT32_MAX);
    auto send_route_data = mutable_request->add_route_data_list();
    send_route_data->CopyFrom(cl_tls.route_data());
	send_route_data->mutable_node_info()->CopyFrom(g_login_node->node_info());
    mutable_request->set_body(cl_tls.route_msg_body());
    switch (cl_tls.next_route_node_type())
    {
    case kControllerNode: 
	{
        g_login_node->controller_node()->CallMethod(ControllerServiceRouteNodeStringMsgMethod, mutable_request);
    }
    break;
    case kDatabaseNode:
    {
        g_login_node->db_node()->CallMethod(DbServiceRouteNodeStringMsgMethod, mutable_request);
    }
    break;
    default:
	{
		LOG_ERROR << "route to next node type error " << request->DebugString() << "," << cl_tls.next_route_node_type();
	}
    break;
    }
	cl_tls.set_next_route_node_id(UINT32_MAX);
///<<< END WRITING YOUR CODE
}

void LoginServiceImpl::RoutePlayerStringMsg(::google::protobuf::RpcController* controller,
    const ::RoutePlayerMsgStringRequest* request,
    ::RoutePlayerMsgStringResponse* response,
    ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

///<<<rpc end
