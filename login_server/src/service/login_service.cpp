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


void LoginServiceImpl::LoginAccountMsReplied(LoginMasterRP d)
{
	//只连接不登录,占用连接
	// login process
	// check account rule: empty , errno
	// check string rule
	auto cit = connections_.find(d->s_rq_.session_id());
	if (cit == connections_.end())
	{
		d->c_rp_->mutable_error()->set_id(kRetLoignCreatePlayerConnectionHasNotAccount);
		return;
	}
	auto& account = d->s_rq_.account();
	auto& response = d->c_rp_;

	//has data
	{
		auto& player = reg.emplace<PlayerPtr>(cit->second, std::make_shared<AccountPlayer>());
		auto ret = player->Login();
		if (ret != kRetOK)
		{
			response->mutable_error()->set_id(ret);
			return;
		}
		auto& account_data = player->account_data();
		redis_->Load(account_data, account);
		if (!account_data.password().empty())
		{
			response->mutable_account_player()->CopyFrom(account_data);
			player->OnDbLoaded();
			return;
		}
	}
	// database process
	auto c(std::make_shared<LoginRpcReplied::element_type>(*d));
	c->s_rq_.set_account(account);
	c->s_rq_.set_session_id(d->s_rq_.session_id());
	l2db_login_stub_.CallMethodString(&LoginServiceImpl::LoginAccountDbReplied, c, this, &dbservice::DbService_Stub::Login);
}

void LoginServiceImpl::LoginAccountDbReplied(LoginRpcReplied d)
{
	auto& srp = d->s_rp_;
	d->c_rp_->mutable_account_player()->CopyFrom(srp->account_player());
	UpdateAccount(d->s_rq_.session_id(), srp->account_player());
}

void LoginServiceImpl::CreatePlayerDbReplied(CreatePlayerRpcReplied d)
{
	auto& srp = d->s_rp_;
	d->c_rp_->mutable_account_player()->CopyFrom(srp->account_player());
	UpdateAccount(d->s_rq_.session_id(), srp->account_player());
}

void LoginServiceImpl::EnterGameDbReplied(EnterGameDbRpcReplied d)
{
	//db 加载过程中断线了
	auto& srq = d->s_rq_;
	auto cit = connections_.find(d->c_rp_->session_id());
	if (cit == connections_.end())
	{
		return;
	}
	::gw2l::EnterGameResponse* response = nullptr;
	::google::protobuf::Closure* done = nullptr;
	d->Move(response, done);
	EnterMS(srq.player_id(), response->session_id(), response, done);
}

void LoginServiceImpl::EnterMsReplied(EnterGameMSRpcReplied d)
{
	connections_.erase(d->s_rq_.session_id());
}

void LoginServiceImpl::EnterMS(Guid player_id,
	uint64_t session_id,
	::gw2l::EnterGameResponse* response,
	::google::protobuf::Closure* done)
{
	auto it = connections_.find(session_id);
	if (connections_.end() == it)
	{
		ReturnCloseureError(kRetLoginEnterGameConnectionAccountEmpty);
	}
	auto cp(std::make_shared<EnterGameMSRpcReplied::element_type>(response, done));
	cp->s_rq_.set_player_id(player_id);
	cp->s_rq_.set_session_id(response->session_id());
	ms_node_stub_.CallMethodString(
		&LoginServiceImpl::EnterMsReplied,
		cp,
		this,
		&msservice::MasterNodeService_Stub::OnLsEnterGame);
}

void LoginServiceImpl::UpdateAccount(uint64_t session_id, const ::account_database& a_d)
{
	auto cit = connections_.find(session_id);
	if (cit == connections_.end())//断线了
	{
		return;
	}
	auto* p_player = reg.try_get<PlayerPtr>(cit->second);
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
	auto c(std::make_shared<LoginMasterRP::element_type>(response, done));
	auto& s_reqst = c->s_rq_;
	s_reqst.set_account(request->account());
	s_reqst.set_session_id(request->session_id());
	auto it = connections_.emplace(request->session_id(), EntityPtr());
	if (it.first != connections_.end())
	{
		reg.emplace_or_replace<std::string>(it.first->second, request->account());
	}
	ms_node_stub_.CallMethodString( &LoginServiceImpl::LoginAccountMsReplied, c, this, &msservice::MasterNodeService_Stub::OnLsLoginAccount);
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
	auto cit = connections_.find(request->session_id());
	if (cit == connections_.end())
	{
		ReturnCloseureError(kRetLoignCreatePlayerConnectionHasNotAccount);
	}
	auto* p_player = reg.try_get<PlayerPtr>(cit->second);
	if (nullptr == p_player)
	{
		ReturnCloseureError(kRetLoginCreateConnectionAccountEmpty);
	}
	auto& ap = *p_player;
	CheckReturnCloseureError(ap->CreatePlayer());

	// database process
	auto c(std::make_shared<CreatePlayerRpcReplied::element_type>(response, done));
	c->s_rq_.set_session_id(request->session_id());
	c->s_rq_.set_account(ap->account());
	l2db_login_stub_.CallMethodString(
		&LoginServiceImpl::CreatePlayerDbReplied,
		c,
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
	auto cit = connections_.find(session_id);
	if (cit == connections_.end())
	{
		ReturnCloseureError(kRetLoginEnterGameConnectionAccountEmpty);
	}
	auto* p_player = reg.try_get<PlayerPtr>(cit->second);
	if (nullptr == p_player)
	{
		ReturnCloseureError(kRetLoginEnterGameConnectionAccountEmpty);
	}
	// check second times change player id error 
	auto& ap = *p_player;
	CheckReturnCloseureError(ap->EnterGame());

	// long time in login processing
	auto player_id = request->player_id();
	if (!ap->IsInPlayerList(player_id))
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
		EnterMS(player_id, session_id, response, done);
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
	auto cit = connections_.find(request->session_id());
	if (cit == connections_.end())
	{
		LOG_ERROR << " leave game not found connection";
		return;
	}
	msservice::LsLeaveGameRequest ms_request;
	ms_request.set_session_id(request->session_id());
	ms_node_stub_.CallMethod(ms_request,
		&msservice::MasterNodeService_Stub::OnLsLeaveGame);
	connections_.erase(cit);
///<<< END WRITING YOUR CODE 
}

void LoginServiceImpl::Disconnect(::google::protobuf::RpcController* controller,
    const gw2l::DisconnectRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	connections_.erase(request->session_id());
	msservice::LsDisconnectRequest message;
	message.set_session_id(request->session_id());
	ms_node_stub_.CallMethod(message,
		&msservice::MasterNodeService_Stub::OnLsDisconnect);	
///<<< END WRITING YOUR CODE 
}

///<<<rpc end
