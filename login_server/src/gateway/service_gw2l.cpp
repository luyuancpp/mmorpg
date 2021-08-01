#include "service_gw2l.h"

#include "muduo/base/Logging.h"
#include "src/server_common/rpc_server.h"
#include "src/return_code/return_notice_code.h"
#include "src/server_common/closure_auto_done.h"

using namespace muduo;
using namespace muduo::net;

namespace gw2l
{
    LoginServiceImpl::LoginServiceImpl(LoginStubl2ms& l2ms_login_stub,
        LoginStubl2db& l2db_login_stub)
        : l2ms_login_stub_(l2ms_login_stub),
          l2db_login_stub_(l2db_login_stub)
    {}

void LoginServiceImpl::Login(::google::protobuf::RpcController* controller,
    const gw2l::LoginRequest* request,
    gw2l::LoginResponse* response,
    ::google::protobuf::Closure* done)
{
    //ֻ���Ӳ���¼,ռ������
    // login process
    // check account rule
    //check string rule
    {
        auto it = login_players_.find(request->account());
        if (it == login_players_.end())
        {
            assert(connection_accounts_.find(request->connection_id()) == connection_accounts_.end());
            auto ret = login_players_.emplace(request->account(), std::make_shared<AccountPlayer>());
            it = ret.first;
        }
        auto& player = it->second;
        CheckReturnCloseureError(player->Login());
        connection_accounts_.emplace(request->connection_id(), player);
        auto& account_data = player->account_data();
        redis_->Load(account_data, request->account());
        if (!account_data.password().empty())
        {
            response->mutable_account_player()->CopyFrom(account_data);
            player->OnDbLoaded();
            ReturnCloseureOK;
        }
    }
 
    // database process
    LoginRP cp(std::make_shared<LoginRpcString>(response, done));
    auto& s_reqst = cp->s_reqst_;
    s_reqst.set_account(request->account());
    s_reqst.set_password(request->password());
    l2db_login_stub_.CallMethodString(this, &LoginServiceImpl::DbLoginReplied, cp,  &l2db::LoginService_Stub::Login);
}

void LoginServiceImpl::DbLoginReplied(LoginRP d)
{
    auto& sresp = d->s_resp_;
    d->c_resp_->mutable_account_player()->CopyFrom(sresp->account_player());
    UpdateAccount(d->s_reqst_.account(), sresp->account_player());
}

void LoginServiceImpl::CreatPlayer(::google::protobuf::RpcController* controller,
    const gw2l::CreatePlayerRequest* request, 
    gw2l::CreatePlayerResponse* response, 
    ::google::protobuf::Closure* done)
{
    // login process
    //check name rule
    auto cit = connection_accounts_.find(request->connection_id());
    if (cit == connection_accounts_.end())
    {
        ReturnCloseureError(common::RET_LOGIN_CREATE_PLAYER_CONNECTION_HAS_NOT_ACCOUNT);
    }
    auto& ap = cit->second;
    CheckReturnCloseureError(ap->CreatePlayer());

    // database process
    CreatePlayerRP cp(std::make_shared<CreatePlayerRpcString>(response, done));
    cp->s_reqst_.set_account(cit->second->account());
    l2db_login_stub_.CallMethodString(this,
        &LoginServiceImpl::DbCreatePlayerReplied,
        cp,
        &l2db::LoginService_Stub::CreatePlayer);
}

void LoginServiceImpl::DbCreatePlayerReplied(CreatePlayerRP d)
{
    auto& sresp = d->s_resp_;
    d->c_resp_->mutable_account_player()->CopyFrom(sresp->account_player());
    UpdateAccount(d->s_reqst_.account(), sresp->account_player());
}

void LoginServiceImpl::EnterGame(::google::protobuf::RpcController* controller,
    const ::gw2l::EnterGameRequest* request,
    ::gw2l::EnterGameResponse* response,
    ::google::protobuf::Closure* done)
{
    auto player_id = request->player_id();
    auto connection_id = request->connection_id();
    auto cit = connection_accounts_.find(connection_id);
    if (cit == connection_accounts_.end())
    {
        ReturnCloseureError(common::RET_LOGIN_CREATE_PLAYER_CONNECTION_HAS_NOT_ACCOUNT);
    }
    auto& ap = cit->second;
    // check second times change player id error 
    CheckReturnCloseureError(ap->EnterGame());

    // long time in login processing
    if (!ap->IsPlayerId(player_id))
    {
        ReturnCloseureError(common::RET_LOGIN_ENTER_GAME_PLAYER_ID);
    }
    auto& account = ap->account();
    // player in redis return ok
    player_database new_player;
    redis_->Load(new_player, player_id);
    ap->Playing(player_id);//test
    response->set_connection_id(connection_id);
    response->set_player_id(player_id);//test
    if (new_player.player_id() > 0)
    {
        ReturnCloseureOK;
    }        
    // database to redis 
    EnterGameRP cp(std::make_shared<EnterGameRpcString>(response, done));
    auto& sreqst = cp->s_reqst_;
    sreqst.set_account(account);
    sreqst.set_player_id(player_id);
    l2db_login_stub_.CallMethodString(this,
        &LoginServiceImpl::EnterGameDbReplied,
        cp,
        &l2db::LoginService_Stub::EnterGame);
}

void LoginServiceImpl::EnterGameDbReplied(EnterGameRP d)
{
}

void LoginServiceImpl::EnterMasterServer(::google::protobuf::RpcController* controller,
    const ::gw2l::EnterMasterRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{    
    common::ClosurePtr cp(done);
    auto connection_id = request->connection_id();
    auto cit = connection_accounts_.find(connection_id);
    if (cit == connection_accounts_.end())
    {
        LOG_ERROR << "not find player";
        return;
    }
    auto& ap = cit->second;
    l2ms::EnterGameRequest sreqst;
    sreqst.set_account(ap->account());
    sreqst.set_player_id(request->player_id());
    sreqst.set_connection_id(connection_id);
    l2ms_login_stub_.CallMethod(        
        sreqst,
        &l2ms::LoginService_Stub::EnterGame);
}

void LoginServiceImpl::Disconnect(::google::protobuf::RpcController* controller, 
    const ::gw2l::DisconnectRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    common::ClosurePtr cp(done);
    auto cit = connection_accounts_.find(request->connection_id());
    if (cit == connection_accounts_.end())
    {
        return;
    }
    login_players_.erase(cit->second->account());
    connection_accounts_.erase(cit);    
}

void LoginServiceImpl::UpdateAccount(const std::string& a, const ::account_database& a_d)
{
    auto it = login_players_.find(a);
    if (it == login_players_.end())
    {
        LOG_ERROR << "account empty " << a;
        return;
    }
    auto& ap = it->second;
    ap->set_account_data(a_d);
    ap->OnDbLoaded();
}
}  // namespace gw2l
