#include "login_service.h"
#include "src/network/rpc_closure.h"
///<<< BEGIN WRITING YOUR CODE  
#include "muduo/base/Logging.h"

#include "src/game_logic/game_registry.h"
#include "src/network/rpc_server.h"
#include "src/game_logic/tips_id.h"
#include "src/login_server.h"

using namespace muduo;
using namespace muduo::net;

LoginServiceImpl::LoginServiceImpl(LoginStubl2ms& l2ms_login_stub,
	LoginStubl2db& l2db_login_stub)
	: ms_node_stub_(l2ms_login_stub),
	l2db_login_stub_(l2db_login_stub)
{}


void LoginServiceImpl::LoginAccountMsReplied(LoginAcountMsRpc replied)
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
		redis_->Load(account_data, replied->s_rq_.account());
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
	l2db_login_stub_.CallMethodString(&LoginServiceImpl::LoginAccountDbReplied, rpc, this, &dbservice::DbService_Stub::Login);
}

void LoginServiceImpl::LoginAccountDbReplied(LoginAccountDbRpc replied)
{
	auto& srp = replied->s_rp_;
	replied->c_rp_->mutable_account_player()->CopyFrom(srp->account_player());
	UpdateAccount(replied->s_rq_.session_id(), srp->account_player());
}

void LoginServiceImpl::CreatePlayerDbReplied(CreatePlayerRpcReplied replied)
{
	auto& srp = replied->s_rp_;
	replied->c_rp_->mutable_account_player()->CopyFrom(srp->account_player());
	UpdateAccount(replied->s_rq_.session_id(), srp->account_player());
}

void LoginServiceImpl::EnterGameDbReplied(EnterGameDbRpcReplied replied)
{
	//db 加载过程中断线了
	auto& srq = replied->s_rq_;
	auto sit = sessions_.find(replied->c_rp_->session_id());
	if (sit == sessions_.end())
	{
		return;
	}
	::gw2l::EnterGameResponse* response = nullptr;
	::google::protobuf::Closure* done = nullptr;
	replied->Move(response, done);
	EnterGame(srq.player_id(), response->session_id(), response, done);
}

void LoginServiceImpl::EnterGameReplied(EnterGameMsRpc replied)
{
	sessions_.erase(replied->s_rq_.session_id());
}

void LoginServiceImpl::EnterGame(Guid player_id,
	uint64_t session_id,
	::gw2l::EnterGameResponse* response,
	::google::protobuf::Closure* done)
{
	auto it = sessions_.find(session_id);
	if (sessions_.end() == it)
	{
		ReturnCloseureError(kRetLoginEnterGameConnectionAccountEmpty);
	}
	auto rpc(std::make_shared<EnterGameMsRpc::element_type>(response, done));
	rpc->s_rq_.set_player_id(player_id);
	rpc->s_rq_.set_session_id(response->session_id());
	ms_node_stub_.CallMethodString(
		&LoginServiceImpl::EnterGameReplied,
		rpc,
		this,
		&msservice::MasterNodeService_Stub::OnLsEnterGame);
}

void LoginServiceImpl::UpdateAccount(uint64_t session_id, const ::account_database& a_d)
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
///<<< END WRITING YOUR CODE

///<<<rpc begin
void LoginServiceImpl::Login(::google::protobuf::RpcController* controller,
    const gw2l::LoginRequest* request,
    gw2l::LoginResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	d.SelfDelete();
	//测试用例连接不登录马上断线，
	//账号登录马上在redis 里面，考虑第一天注册很多账号的时候账号内存很多，何时回收
	//登录的时候马上断开连接换了个gate应该可以登录成功
	//login master
	auto rpc(std::make_shared<LoginAcountMsRpc::element_type>(response, done));
	auto& s_reqst = rpc->s_rq_;
	s_reqst.set_account(request->account());
	s_reqst.set_session_id(request->session_id());
	sessions_.emplace(request->session_id(), EntityPtr());
	ms_node_stub_.CallMethodString( &LoginServiceImpl::LoginAccountMsReplied, rpc, this, &msservice::MasterNodeService_Stub::OnLsLoginAccount);
///<<< END WRITING YOUR CODE 
}

void LoginServiceImpl::CreatPlayer(::google::protobuf::RpcController* controller,
    const gw2l::CreatePlayerRequest* request,
    gw2l::CreatePlayerResponse* response,
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
	auto rpc(std::make_shared<CreatePlayerRpcReplied::element_type>(response, done));
	rpc->s_rq_.set_session_id(request->session_id());
	rpc->s_rq_.set_account(ap->account());
	l2db_login_stub_.CallMethodString(
		&LoginServiceImpl::CreatePlayerDbReplied,
		rpc,
		this,
		&dbservice::DbService_Stub::CreatePlayer);
///<<< END WRITING YOUR CODE 
}

void LoginServiceImpl::EnterGame(::google::protobuf::RpcController* controller,
    const gw2l::EnterGameRequest* request,
    gw2l::EnterGameResponse* response,
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
	redis_->Load(new_player, player_id);
	ap->Playing(player_id);//test
	response->set_session_id(session_id);
	response->set_player_id(player_id);//test
	if (new_player.player_id() > 0)
	{
		EnterGame(player_id, session_id, response, done);
		return;
	}
	// database to redis 
	auto c(std::make_shared<EnterGameDbRpcReplied::element_type>(response, done));
	auto& srq = c->s_rq_;
	srq.set_player_id(player_id);
	l2db_login_stub_.CallMethodString(
		&LoginServiceImpl::EnterGameDbReplied,
		c,
		this,
		&dbservice::DbService_Stub::EnterGame);
///<<< END WRITING YOUR CODE 
}

void LoginServiceImpl::LeaveGame(::google::protobuf::RpcController* controller,
    const gw2l::LeaveGameRequest* request,
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
	msservice::LsLeaveGameRequest ms_request;
	ms_request.set_session_id(request->session_id());
	ms_node_stub_.CallMethod(ms_request,
		&msservice::MasterNodeService_Stub::OnLsLeaveGame);
	sessions_.erase(sit);
///<<< END WRITING YOUR CODE 
}

void LoginServiceImpl::Disconnect(::google::protobuf::RpcController* controller,
    const gw2l::DisconnectRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	sessions_.erase(request->session_id());
	msservice::LsDisconnectRequest message;
	message.set_session_id(request->session_id());
	ms_node_stub_.CallMethod(message,
		&msservice::MasterNodeService_Stub::OnLsDisconnect);	
///<<< END WRITING YOUR CODE 
}

///<<<rpc end
