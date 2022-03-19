#include "l2ms.h"
///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"

#include "src/game_logic/comp/player_comp.hpp"
#include "src/game_logic/game_registry.h"
#include "src/network/gate_node.h"
#include "src/network/gs_node.h"
#include "src/master_player/ms_player_list.h"
#include "src/master_server.h"
#include "src/return_code/error_code.h"
#include "src/server_common/closure_auto_done.h"
#include "src/sys/servernode_sys.hpp"
#include "src/game_logic/comp/gs_scene_comp.hpp"
#include "src/sys/message_sys.hpp"

#include "ms2gw.pb.h"
#include "ms2gs.pb.h"

using namespace master;
using namespace common;

using Ms2GsStubPtr = std::unique_ptr <common::RpcStub<ms2gs::Ms2gService_Stub>>;

std::size_t kMaxPlayerSize = 1000;

///<<< END WRITING YOUR CODE

namespace l2ms{
///<<< BEGIN WRITING YOUR CODE
void LoginServiceImpl::Ms2gsEnterGameReplied(Ms2gsEnterGameRpcRplied replied)
{
	ms2gw::PlayerEnterGSRequest gw_request;
	//gw_request.set_connection_id(connection_id);//error
}
///<<< END WRITING YOUR CODE

///<<<rpc begin
void LoginServiceImpl::LoginAccount(::google::protobuf::RpcController* controller,
    const l2ms::LoginAccountRequest* request,
    l2ms::LoginAccountResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE LoginAccount
    auto lit = logined_accounts_.find(request->account());
    if (lit == logined_accounts_.end() && 
        (PlayerList::GetSingleton().player_size() + logined_accounts_.size()) >= kMaxPlayerSize)
    {
        //如果登录的是新账号,满了得去排队
        response->mutable_error()->set_error_no(RET_LOGIN_MAX_PLAYER_SIZE);
        return;
    }
        
    if (lit != logined_accounts_.end())
    {
        auto& lc = lit->second;
        //如果不是同一个登录服务器,踢掉已经登录的账号
        if (reg.get<AccountLoginNode>(lc.entity()).login_node_id_ != request->login_node_id())
        {

        }
        else//告诉客户端登录中
        {
            response->mutable_error()->set_error_no(RET_LOGIN_LOGIN_ING);
        }
    }
    else
    {
        auto result = logined_accounts_.emplace(request->account(), MSLoginAccount());
        if (result.second)
        {
            auto& lc = result.first->second;
            reg.emplace<SharedAccountString>(lc.entity(), std::make_shared<std::string>(request->account()));
            reg.emplace<AccountLoginNode>(lc.entity(), AccountLoginNode{ request->login_node_id(), request->gate_node_id()});
        }
    }
///<<< END WRITING YOUR CODE LoginAccount
}

void LoginServiceImpl::EnterGame(::google::protobuf::RpcController* controller,
    const l2ms::EnterGameRequest* request,
    l2ms::EnterGameResponese* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE EnterGame
    auto guid = request->guid();   
    auto e = reg.create();
    reg.emplace<Guid>(e, guid);
    reg.emplace<GateConnectionId>(e, request->connection_id());

	auto& gate_nodes = reg.get<GateNodes>(global_entity());
    auto gate_it = gate_nodes.find(request->gate_node_id());
	if (gate_it != gate_nodes.end())
	{
        auto gate = reg.try_get<GateNodePtr>(gate_it->second);
        if (nullptr != gate)
        {
            reg.emplace<GateNodePtr>(e, *gate);
        }       
	}

    PlayerList::GetSingleton().EnterGame(guid, e);

    GetSceneParam getp;
    getp.scene_confid_ = 1;
    auto scene = ServerNodeSystem::GetMainSceneNotFull(getp);
    if (scene == entt::null)
    {
        // todo default
        LOG_INFO << "player " << guid << " enter default secne";
    }
    auto* p_gs_data = reg.try_get<GSDataPtrComp>(scene);
    if (nullptr == p_gs_data)
    {
		// todo default
		LOG_INFO << "player " << guid << " enter default secne";
		return;
    }
    auto gs_node_id = (*p_gs_data)->node_id();

	auto& gs_nodes = common::reg.get<master::GsNodes>(master::global_entity());
	auto it = gs_nodes.find(gs_node_id);
	if (it != gs_nodes.end())
	{
        Ms2gsEnterGameRpcRplied msg;
        msg.s_rq_.set_player_id(guid);
		reg.get<Ms2GsStubPtr>(it->second)->CallMethod1(&LoginServiceImpl::Ms2gsEnterGameReplied,
			msg,
			this,
            &ms2gs::Ms2gService_Stub::EnterGame);
	}
///<<< END WRITING YOUR CODE EnterGame
}

void LoginServiceImpl::LeaveGame(::google::protobuf::RpcController* controller,
    const l2ms::LeaveGameRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE LeaveGame
    auto guid = request->guid();
    auto e = PlayerList::GetSingleton().GetPlayer(guid);
    assert(reg.get<Guid>(e) == guid);
    reg.destroy(e);
    PlayerList::GetSingleton().LeaveGame(guid);  
    assert(!PlayerList::GetSingleton().HasPlayer(guid));
    assert(PlayerList::GetSingleton().GetPlayer(guid) == entt::null); 
///<<< END WRITING YOUR CODE LeaveGame
}

void LoginServiceImpl::Disconect(::google::protobuf::RpcController* controller,
    const l2ms::DisconnectRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE Disconect
    auto guid = request->guid();
    auto e = PlayerList::GetSingleton().GetPlayer(guid);
    if (entt::null  == e)
    {
        return;
    }
    assert(reg.get<Guid>(e) == guid);
    reg.destroy(e);
    PlayerList::GetSingleton().LeaveGame(guid);
    assert(!PlayerList::GetSingleton().HasPlayer(guid));
    assert(PlayerList::GetSingleton().GetPlayer(guid) == entt::null);
///<<< END WRITING YOUR CODE Disconect
}

///<<<rpc end
}// namespace l2ms
