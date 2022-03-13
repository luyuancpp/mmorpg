#include "gw2l.h"
///<<< BEGIN WRITING YOUR CODE  
#include "muduo/base/Logging.h"

#include "src/game_logic/game_registry.h"
#include "src/server_common/rpc_server.h"
#include "src/server_common/closure_auto_done.h"
#include "src/return_code/error_code.h"
#include "src/login_server.h"

using namespace muduo;
using namespace muduo::net;
using namespace common;
///<<< END WRITING YOUR CODE

namespace gw2l{
 ///<<< BEGIN WRITING YOUR CODE 
LoginServiceImpl::LoginServiceImpl(LoginStubl2ms& l2ms_login_stub,
    LoginStubl2db& l2db_login_stub)
    : l2ms_login_stub_(l2ms_login_stub),
        l2db_login_stub_(l2db_login_stub)
{}


void LoginServiceImpl::LoginAccountMSReplied(LoginMasterRP d)
{
	//只连接不登录,占用连接
	// login process
	// check account rule: empty , errno
	//check string rule
	auto cit = connections_.find(d->s_rq_.connection_id());
	if (cit == connections_.end())
	{
		d->c_rp_->mutable_error()->set_error_no(RET_LOGIN_CREATE_PLAYER_CONNECTION_HAS_NOT_ACCOUNT);
		return;
	}
	auto& account = d->s_rq_.account();
	auto& response = d->c_rp_;

	//has dat
	{
		auto& player = reg.emplace<PlayerPtr>(cit->second.entity(), std::make_shared<AccountPlayer>());
		auto ret = player->Login();
		if (ret != RET_OK)
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
	c->s_rq_.set_connection_id(d->s_rq_.connection_id());
	l2db_login_stub_.CallMethodString(this, &LoginServiceImpl::LoginAccountDbReplied, c, &l2db::LoginService_Stub::Login);
}

void LoginServiceImpl::LoginAccountDbReplied(LoginRpcReplied d)
{
	auto& srp = d->s_rp_;
	d->c_rp_->mutable_account_player()->CopyFrom(srp->account_player());
	UpdateAccount(d->s_rq_.connection_id(), srp->account_player());
}

void LoginServiceImpl::CreatePlayerDbReplied(CreatePlayerRpcReplied d)
{
	auto& srp = d->s_rp_;
	d->c_rp_->mutable_account_player()->CopyFrom(srp->account_player());
	UpdateAccount(d->s_rq_.connection_id(), srp->account_player());
}

void LoginServiceImpl::EnterGameDbReplied(EnterGameDbRpcReplied d)
{
	//db 加载过程中断线了
	auto& srq = d->s_rq_;
	auto cit = connections_.find(d->c_rp_->connection_id());
	if (cit == connections_.end())
	{
		return;
	}
	::gw2l::EnterGameResponse* response = nullptr;
	::google::protobuf::Closure* done = nullptr;
	d->Move(response, done);
	EnterMS(srq.guid(), response->connection_id(), response, done);
}

void LoginServiceImpl::EnterMSReplied(EnterGameMSRpcReplied d)
{
	connections_.erase(d->s_rq_.connection_id());
}

void LoginServiceImpl::EnterMS(common::Guid guid,
	uint64_t connection_id,
	::gw2l::EnterGameResponse* response,
	::google::protobuf::Closure* done)
{
	auto it = connections_.find(connection_id);
	if (connections_.end() == it)
	{
		ReturnCloseureError(REG_LOGIN_ENTERGAMEE_CONNECTION_ACCOUNT_EMPTY);
	}
	auto cp(std::make_shared<EnterGameMSRpcReplied::element_type>(response, done));
	cp->s_rq_.set_guid(guid);
	cp->s_rq_.set_connection_id(response->connection_id());
	l2ms_login_stub_.CallMethodString(this,
		&LoginServiceImpl::EnterMSReplied,
		cp,
		&l2ms::LoginService_Stub::EnterGame);
}

void LoginServiceImpl::UpdateAccount(uint64_t connection_id, const ::account_database& a_d)
{
	auto cit = connections_.find(connection_id);
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
    //login master
    auto c(std::make_shared<LoginMasterRP::element_type>(response, done));
    auto& s_reqst = c->s_rq_;
    s_reqst.set_account(request->account());
    s_reqst.set_login_node_id(g_login_server->node_id());
    s_reqst.set_connection_id(request->connection_id());
    auto it =  connections_.emplace(request->connection_id(), common::EntityPtr());
    if (it.second)
    {
        reg.emplace<std::string>(it.first->second.entity(), request->account());
    }
    l2ms_login_stub_.CallMethodString(this, &LoginServiceImpl::LoginAccountMSReplied, c, &l2ms::LoginService_Stub::LoginAccount);
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
    auto cit = connections_.find(request->connection_id());
    if (cit == connections_.end())
    {
        ReturnCloseureError(RET_LOGIN_CREATE_PLAYER_CONNECTION_HAS_NOT_ACCOUNT);
    }
    auto* p_player = reg.try_get<PlayerPtr>(cit->second.entity());
    if (nullptr == p_player)
    {
        ReturnCloseureError(REG_LOGIN_CREATEPLAYER_CONNECTION_ACCOUNT_EMPTY);
    }
    auto& ap = *p_player;
    CheckReturnCloseureError(ap->CreatePlayer());

    // database process
    auto c(std::make_shared<CreatePlayerRpcReplied::element_type>(response, done));
    c->s_rq_.set_connection_id(request->connection_id());
    c->s_rq_.set_account(ap->account());
    l2db_login_stub_.CallMethodString(this,
        &LoginServiceImpl::CreatePlayerDbReplied,
        c,
        &l2db::LoginService_Stub::CreatePlayer);
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
	auto connection_id = request->connection_id();
	auto cit = connections_.find(connection_id);
	if (cit == connections_.end())
	{
		ReturnCloseureError(REG_LOGIN_ENTERGAMEE_CONNECTION_ACCOUNT_EMPTY);
	}
	auto* p_player = reg.try_get<PlayerPtr>(cit->second.entity());
	if (nullptr == p_player)
	{
		ReturnCloseureError(REG_LOGIN_CREATEPLAYER_CONNECTION_ACCOUNT_EMPTY);
	}
	// check second times change player id error 
	auto& ap = *p_player;
	CheckReturnCloseureError(ap->EnterGame());

	// long time in login processing
	auto guid = request->guid();
	if (!ap->IsInPlayerList(guid))
	{
		ReturnCloseureError(RET_LOGIN_ENTER_GUID);
	}
	// player in redis return ok
	player_database new_player;
	redis_->Load(new_player, guid);
	ap->Playing(guid);//test
	response->set_connection_id(connection_id);
	response->set_guid(guid);//test
	if (new_player.guid() > 0)
	{
		EnterMS(guid, connection_id, response, done);
		return;
	}
	// database to redis 
	auto c(std::make_shared<EnterGameDbRpcReplied::element_type>(response, done));
	auto& srq = c->s_rq_;
	srq.set_guid(guid);
	l2db_login_stub_.CallMethodString(this,
		&LoginServiceImpl::EnterGameDbReplied,
		c,
		&l2db::LoginService_Stub::EnterGame);
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
	auto cit = connections_.find(request->connection_id());
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
	l2ms::LeaveGameRequest ms_request;
	ms_request.set_guid(player->PlayingId());
	l2ms_login_stub_.CallMethod(ms_request,
		&l2ms::LoginService_Stub::LeaveGame);
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
	auto cit = connections_.find(request->connection_id());
	if (cit == connections_.end())//连接并没有登录
	{
		return;
	}
	//连接已经登录过
	auto* p_player = reg.try_get<PlayerPtr>(cit->second.entity());
	if (nullptr == p_player)
	{
		return;
	}
	auto& player = (*p_player);
	l2ms::DisconnectRequest ms_disconnect;
	ms_disconnect.set_guid(player->PlayingId());
	l2ms_login_stub_.CallMethod(ms_disconnect,
		&l2ms::LoginService_Stub::Disconect);
	connections_.erase(cit);
///<<< END WRITING YOUR CODE Disconnect
}

///<<<rpc end
}// namespace gw2l
