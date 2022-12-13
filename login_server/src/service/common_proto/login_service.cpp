#include "login_service.h"
#include "src/network/rpc_closure.h"
///<<< BEGIN WRITING YOUR CODE  
#include "muduo/base/Logging.h"

#include "src/game_logic/game_registry.h"
#include "src/network/rpc_server.h"
#include "src/game_logic/tips_id.h"
#include "src/login_server.h"
#include "src/comp/account_player.h"
#include "src/network/rpc_closure.h"
#include "src/network/rpc_client.h"
#include "src/redis_client/redis_client.h"
#include "src/pb/pbc/service_method/controller_servicemethod.h"

#include "login_service.pb.h"
#include "database_service.pb.h"
#include "controller_service.pb.h"

using namespace muduo;
using namespace muduo::net;

using PlayerPtr = std::shared_ptr<AccountPlayer>;
using LoginPlayersMap = std::unordered_map<std::string, PlayerPtr>;
using ConnectionEntityMap = std::unordered_map<Guid, EntityPtr>;

ConnectionEntityMap sessions_;

using EnterGameControllerRpc = std::shared_ptr<RpcString<ControllerNodeEnterGameRequest, ControllerNodeEnterGameResponese, LoginNodeEnterGameResponse>>;
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
		ReturnCloseureError(kRetLoginEnterGameConnectionAccountEmpty);
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
	auto* p_player = registry.try_get<PlayerPtr>(sit->second);
	if (nullptr == p_player)
	{
		return;
	}
	auto& ap = *p_player;
	ap->set_account_data(a_d);
	ap->OnDbLoaded();
}

using LoginAccountDbRpc = std::shared_ptr< RpcString<DatabaseNodeLoginRequest, DatabaseNodeLoginResponse, LoginNodeLoginResponse>>;
void LoginAccountDbReplied(LoginAccountDbRpc replied)
{
	auto& srp = replied->s_rp_;
	replied->c_rp_->mutable_account_player()->CopyFrom(srp->account_player());
	UpdateAccount(replied->s_rq_.session_id(), srp->account_player());
}

using LoginAcountControllerRpc = std::shared_ptr<RpcString<ControllerNodeLoginAccountRequest, ControllerNodeLoginAccountResponse, LoginNodeLoginResponse>>;
void LoginAccountControllerReplied(LoginAcountControllerRpc replied)
{
	//只连接不登录,占用连接
	// login process
	// check account rule: empty , errno
	// check string rule
	auto sit = sessions_.find(replied->s_rq_.session_id());
	if (sit == sessions_.end())
	{
		replied->c_rp_->mutable_error()->set_id(kRetLoignCreatePlayerConnectionHasNotAccount);
		return;
	}
	//has data
	{
		auto& player = registry.emplace<PlayerPtr>(sit->second, std::make_shared<PlayerPtr::element_type>());
		auto ret = player->Login();
		if (ret != kRetOK)
		{
			replied->c_rp_->mutable_error()->set_id(ret);
			return;
		}
		auto& account_data = player->account_data();
		g_login_node->redis_client()->Load(account_data, replied->s_rq_.account());
		if (!account_data.password().empty())
		{
			replied->c_rp_->mutable_account_player()->CopyFrom(account_data);
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
    const ::LoginNodeLoginRequest* request,
    ::LoginNodeLoginResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	d.SelfDelete();
	//测试用例连接不登录马上断线，
	//账号登录马上在redis 里面，考虑第一天注册很多账号的时候账号内存很多，何时回收
	//登录的时候马上断开连接换了个gate应该可以登录成功
	//login controller
	auto rpc(std::make_shared<LoginAcountControllerRpc::element_type>(response, done));
	auto& s_reqst = rpc->s_rq_;
	s_reqst.set_account(request->account());
	s_reqst.set_session_id(request->session_id());
	sessions_.emplace(request->session_id(), EntityPtr());
	//g_login_node->controller_node().CallMethodString1( LoginAccountControllerReplied, rpc, &ControllerService::ControllerNodeService_Stub::OnLsLoginAccount);
///<<< END WRITING YOUR CODE 
}

void LoginServiceImpl::CreatPlayer(::google::protobuf::RpcController* controller,
    const ::LoginNodeCreatePlayerRequest* request,
    ::LoginNodeCreatePlayerResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	d.SelfDelete();
	// login process
	//check name rule
	auto sit = sessions_.find(request->session_id());
	if (sit == sessions_.end())
	{
		ReturnCloseureError(kRetLoignCreatePlayerConnectionHasNotAccount);
	}
	auto* p_player = registry.try_get<PlayerPtr>(sit->second);
	if (nullptr == p_player)
	{
		ReturnCloseureError(kRetLoginCreateConnectionAccountEmpty);
	}
	auto& ap = *p_player;
	CheckReturnCloseureError(ap->CreatePlayer());

	// database process
	auto rpc(std::make_shared<CreatePlayerRpc::element_type>(response, done));
	rpc->s_rq_.set_session_id(request->session_id());
	rpc->s_rq_.set_account(ap->account());
	/*g_login_node->db_node().CallMethodString1(
		CreatePlayerDbReplied,
		rpc,
		&dbservice::DbService_Stub::CreatePlayer);*/
///<<< END WRITING YOUR CODE 
}

void LoginServiceImpl::EnterGame(::google::protobuf::RpcController* controller,
    const ::LoginNodeEnterGameRequest* request,
    ::LoginNodeEnterGameResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	d.SelfDelete();
	auto session_id = request->session_id();
	auto sit = sessions_.find(session_id);
	if (sit == sessions_.end())
	{
		ReturnCloseureError(kRetLoginEnterGameConnectionAccountEmpty);
	}
	auto* p_player = registry.try_get<PlayerPtr>(sit->second);
	if (nullptr == p_player)
	{
		ReturnCloseureError(kRetLoginEnterGameConnectionAccountEmpty);
	}
	// check second times change player id error 
	auto& ap = *p_player;
	CheckReturnCloseureError(ap->EnterGame());

	// long time in login processing
	auto player_id = request->player_id();
	if (!ap->HasPlayer(player_id))
	{
		ReturnCloseureError(kRetLoginPlayerGuidError);
	}
	// player in redis return ok
	player_database new_player;
	g_login_node->redis_client()->Load(new_player, player_id);
	ap->Playing(player_id);//test
	response->set_session_id(session_id);
	response->set_player_id(player_id);//test
	if (new_player.player_id() > 0)
	{
		::EnterGame(player_id, session_id, response, done);
		return;
	}
	// database to redis 
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
    const ::LoginNodeLeaveGameRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	//连接过，登录过
	auto sit = sessions_.find(request->session_id());
	if (sit == sessions_.end())
	{
		LOG_ERROR << " leave game not found connection";
		return;
	}
	ControllerNodeLsLeaveGameRequest rq;
	rq.set_session_id(request->session_id());
	g_login_node->controller_node()->CallMethod(ControllerServiceOnLsLeaveGameMethodDesc, &rq);
	sessions_.erase(sit);
///<<< END WRITING YOUR CODE 
}

void LoginServiceImpl::Disconnect(::google::protobuf::RpcController* controller,
    const ::LoginNodeDisconnectRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	sessions_.erase(request->session_id());
	ControllerNodeLsDisconnectRequest rq;
	rq.set_session_id(request->session_id());
	g_login_node->controller_node()->CallMethod(ControllerServiceOnLsDisconnectMethodDesc, &rq);
///<<< END WRITING YOUR CODE 
}

void LoginServiceImpl::RouteNodeStringMsg(::google::protobuf::RpcController* controller,
    const ::RouteMsgStringRequest* request,
    ::RouteMsgStringResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

void LoginServiceImpl::RoutePlayerStringMsg(::google::protobuf::RpcController* controller,
    const ::RoutePlayerMsgStringRequest* request,
    ::RoutePlayerMsgStringResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

///<<<rpc end
