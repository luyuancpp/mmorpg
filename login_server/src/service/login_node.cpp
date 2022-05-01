#include "login_node.h"
#include "src/network/rpc_closure.h"
///<<< BEGIN WRITING YOUR CODE  
#include "muduo/base/Logging.h"

#include "src/game_logic/game_registry.h"
#include "src/network/rpc_server.h"
#include "src/return_code/error_code.h"
#include "src/login_server.h"

using namespace muduo;
using namespace muduo::net;
///<<< END WRITING YOUR CODE

///<<< BEGIN WRITING YOUR CODE 
LoginServiceImpl::LoginServiceImpl(LoginStubl2ms& l2ms_login_stub,
	LoginStubl2db& l2db_login_stub)
	: ms_node_stub_(l2ms_login_stub),
	l2db_login_stub_(l2db_login_stub)
{}


void LoginServiceImpl::LoginAccountMSReplied(LoginMasterRP d)
{
	//只连接不登录,占用连接
	// login process
	// check account rule: empty , errno
	// check string rule
	auto cit = connections_.find(d->s_rq_.conn_id());
	if (cit == connections_.end())
	{
		d->c_rp_->mutable_error()->set_error_no(kRetLoignCreatePlayerConnectionHasNotAccount);
		return;
	}
	auto& account = d->s_rq_.account();
	auto& response = d->c_rp_;

	//has data
	{
		auto& player = reg.emplace<PlayerPtr>(cit->second.entity(), std::make_shared<AccountPlayer>());
		auto ret = player->Login();
		if (ret != kRetOK)
		{
			response->mutable_error()->set_error_no(ret);
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
	c->s_rq_.set_conn_id(d->s_rq_.conn_id());
	l2db_login_stub_.CallMethodString(&LoginServiceImpl::LoginAccountDbReplied, c, this, &dbservice::DbService_Stub::Login);
}

void LoginServiceImpl::LoginAccountDbReplied(LoginRpcReplied d)
{
	auto& srp = d->s_rp_;
	d->c_rp_->mutable_account_player()->CopyFrom(srp->account_player());
	UpdateAccount(d->s_rq_.conn_id(), srp->account_player());
}

void LoginServiceImpl::CreatePlayerDbReplied(CreatePlayerRpcReplied d)
{
	auto& srp = d->s_rp_;
	d->c_rp_->mutable_account_player()->CopyFrom(srp->account_player());
	UpdateAccount(d->s_rq_.conn_id(), srp->account_player());
}

void LoginServiceImpl::EnterGameDbReplied(EnterGameDbRpcReplied d)
{
	//db 加载过程中断线了
	auto& srq = d->s_rq_;
	auto cit = connections_.find(d->c_rp_->conn_id());
	if (cit == connections_.end())
	{
		return;
	}
	::gw2l::EnterGameResponse* response = nullptr;
	::google::protobuf::Closure* done = nullptr;
	d->Move(response, done);
	EnterMS(srq.guid(), response->conn_id(), response, done);
}

void LoginServiceImpl::EnterMsReplied(EnterGameMSRpcReplied d)
{
	connections_.erase(d->s_rq_.conn_id());
}

void LoginServiceImpl::EnterMS(Guid guid,
	uint64_t conn_id,
	::gw2l::EnterGameResponse* response,
	::google::protobuf::Closure* done)
{
	auto it = connections_.find(conn_id);
	if (connections_.end() == it)
	{
		ReturnCloseureError(kRetLoginEnterGameConnectionAccountEmpty);
	}
	auto cp(std::make_shared<EnterGameMSRpcReplied::element_type>(response, done));
	cp->s_rq_.set_guid(guid);
	cp->s_rq_.set_conn_id(response->conn_id());
	cp->s_rq_.set_gate_node_id(reg.get<uint32_t>(it->second.entity()));
	cp->s_rq_.set_account(reg.get<std::string>(it->second.entity()));
	ms_node_stub_.CallMethodString(
		&LoginServiceImpl::EnterMsReplied,
		cp,
		this,
		&msservice::MasterNodeService_Stub::OnLsEnterGame);
}

void LoginServiceImpl::UpdateAccount(uint64_t conn_id, const ::account_database& a_d)
{
	auto cit = connections_.find(conn_id);
	if (cit == connections_.end())//断线了
	{
		return;
	}
	auto* p_player = reg.try_get<PlayerPtr>(cit->second.entity());
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
///<<< BEGIN WRITING YOUR CODE Login
	d.SelfDelete();
	//测试用例连接不登录马上断线，
	//账号登录马上在redis 里面，考虑第一天注册很多账号的时候账号内存很多，何时回收
	//登录的时候马上断开连接换了个gate应该可以登录成功
	//login master
	auto c(std::make_shared<LoginMasterRP::element_type>(response, done));
	auto& s_reqst = c->s_rq_;
	s_reqst.set_account(request->account());
	s_reqst.set_login_node_id(g_login_server->login_node_id());
	s_reqst.set_conn_id(request->conn_id());
	s_reqst.set_gate_node_id(request->gate_node_id());
	auto it = connections_.emplace(request->conn_id(), EntityPtr());
	if (it.first != connections_.end())
	{
		reg.emplace_or_replace<std::string>(it.first->second.entity(), request->account());
		reg.emplace_or_replace<uint32_t>(it.first->second.entity(), request->gate_node_id());
	}
	ms_node_stub_.CallMethodString( &LoginServiceImpl::LoginAccountMSReplied, c, this, &msservice::MasterNodeService_Stub::OnLsLoginAccount);
///<<< END WRITING YOUR CODE Login
}

void LoginServiceImpl::CreatPlayer(::google::protobuf::RpcController* controller,
    const gw2l::CreatePlayerRequest* request,
    gw2l::CreatePlayerResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE CreatPlayer
	d.SelfDelete();
	// login process
	//check name rule
	auto cit = connections_.find(request->conn_id());
	if (cit == connections_.end())
	{
		ReturnCloseureError(kRetLoignCreatePlayerConnectionHasNotAccount);
	}
	auto* p_player = reg.try_get<PlayerPtr>(cit->second.entity());
	if (nullptr == p_player)
	{
		ReturnCloseureError(kRetLoginCreateConnectionAccountEmpty);
	}
	auto& ap = *p_player;
	CheckReturnCloseureError(ap->CreatePlayer());

	// database process
	auto c(std::make_shared<CreatePlayerRpcReplied::element_type>(response, done));
	c->s_rq_.set_conn_id(request->conn_id());
	c->s_rq_.set_account(ap->account());
	l2db_login_stub_.CallMethodString(
		&LoginServiceImpl::CreatePlayerDbReplied,
		c,
		this,
		&dbservice::DbService_Stub::CreatePlayer);
///<<< END WRITING YOUR CODE CreatPlayer
}

void LoginServiceImpl::EnterGame(::google::protobuf::RpcController* controller,
    const gw2l::EnterGameRequest* request,
    gw2l::EnterGameResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE EnterGame
	d.SelfDelete();
	auto conn_id = request->conn_id();
	auto cit = connections_.find(conn_id);
	if (cit == connections_.end())
	{
		ReturnCloseureError(kRetLoginEnterGameConnectionAccountEmpty);
	}
	auto* p_player = reg.try_get<PlayerPtr>(cit->second.entity());
	if (nullptr == p_player)
	{
		ReturnCloseureError(kRetLoginEnterGameConnectionAccountEmpty);
	}
	// check second times change player id error 
	auto& ap = *p_player;
	CheckReturnCloseureError(ap->EnterGame());

	// long time in login processing
	auto guid = request->guid();
	if (!ap->IsInPlayerList(guid))
	{
		ReturnCloseureError(kRetLoginPlayerGuidError);
	}
	// player in redis return ok
	player_database new_player;
	redis_->Load(new_player, guid);
	ap->Playing(guid);//test
	response->set_conn_id(conn_id);
	response->set_guid(guid);//test
	if (new_player.guid() > 0)
	{
		EnterMS(guid, conn_id, response, done);
		return;
	}
	// database to redis 
	auto c(std::make_shared<EnterGameDbRpcReplied::element_type>(response, done));
	auto& srq = c->s_rq_;
	srq.set_guid(guid);
	l2db_login_stub_.CallMethodString(
		&LoginServiceImpl::EnterGameDbReplied,
		c,
		this,
		&dbservice::DbService_Stub::EnterGame);
///<<< END WRITING YOUR CODE EnterGame
}

void LoginServiceImpl::LeaveGame(::google::protobuf::RpcController* controller,
    const gw2l::LeaveGameRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE LeaveGame
	//连接过，登录过
	auto cit = connections_.find(request->conn_id());
	if (cit == connections_.end())
	{
		LOG_ERROR << " leave game not found connection";
		return;
	}
	auto* p_player = reg.try_get<PlayerPtr>(cit->second.entity());
	if (nullptr == p_player)
	{
		return;
	}
	auto& player = (*p_player);
	msservice::LsLeaveGameRequest ms_request;
	ms_request.set_guid(player->PlayingId());
	ms_node_stub_.CallMethod(ms_request,
		&msservice::MasterNodeService_Stub::OnLsLeaveGame);
	connections_.erase(cit);
///<<< END WRITING YOUR CODE LeaveGame
}

void LoginServiceImpl::Disconnect(::google::protobuf::RpcController* controller,
    const gw2l::DisconnectRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE Disconnect
	//todo 不同的gate 相同的connect id
	auto cit = connections_.find(request->conn_id());
	if (cit == connections_.end())//连接并没有登录
	{
		return;
	}
	//连接已经登录过

	msservice::LsDisconnectRequest message;
	auto conn = cit->second.entity();
	message.set_account(reg.get<std::string>(conn));
	auto* p_player = reg.try_get<PlayerPtr>(cit->second.entity());
	if (nullptr != p_player)
	{
		auto& player = (*p_player);
		message.set_guid(player->PlayingId());
	}
	ms_node_stub_.CallMethod(message,
		&msservice::MasterNodeService_Stub::OnLsDisconnect);
	connections_.erase(cit);
///<<< END WRITING YOUR CODE Disconnect
}

	///<<<rpc end
