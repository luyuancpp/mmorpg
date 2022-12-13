#include "database_service.h"
#include "src/network/rpc_closure.h"
///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Timestamp.h"

#include "src/mysql_wrapper/mysql_database.h"
#include "src/redis_client/redis_client.h"
#include "src/database_server.h"

#include "comp.pb.h"
///<<< END WRITING YOUR CODE

///<<<rpc begin
void DbServiceImpl::Login(::google::protobuf::RpcController* controller,
    const ::DbNodeLoginRequest* request,
    ::DbNodeLoginResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	::account_database& db = *response->mutable_account_player();
	auto& account = request->account();
	g_db_server->redis_client()->Load(db, account);
	if (response->account_player().password().empty())
	{
		g_db_server->player_mysql_client()->LoadOne(db,
			std::string("account = '") + account + std::string("'"));
	}
	if (!db.password().empty())
	{
		db.set_account(account);
		g_db_server->redis_client()->Save(db, account);
	}
///<<< END WRITING YOUR CODE 
}

void DbServiceImpl::CreatePlayer(::google::protobuf::RpcController* controller,
    const ::DbNodeCreatePlayerRequest* request,
    ::DbNodeCreatePlayerResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	::account_database& r_db = *response->mutable_account_player();
	g_db_server->redis_client()->Load(r_db, request->account());
	player_database new_player;
	new_player.set_register_time(muduo::Timestamp::now().microSecondsSinceEpoch());
	g_db_server->player_mysql_client()->SaveOne(new_player);
	response->set_player_id(g_db_server->player_mysql_client()->LastInsertId());
	new_player.set_player_id(response->player_id());
	r_db.mutable_simple_players()->add_players()->set_player_id(response->player_id());
	g_db_server->redis_client()->Save(new_player, new_player.player_id());
	g_db_server->redis_client()->Save(r_db, r_db.account());
///<<< END WRITING YOUR CODE 
}

void DbServiceImpl::EnterGame(::google::protobuf::RpcController* controller,
    const ::DbNodeEnterGameRequest* request,
    ::DbNodeEnterGameResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE 
	player_database new_player;
	std::string where_case = std::string("player_id = '") +
		std::to_string(request->player_id()) +
		std::string("'");
	g_db_server->player_mysql_client()->LoadOne(new_player, where_case);
	assert(new_player.player_id() > 0);
	g_db_server->redis_client()->Save(new_player, new_player.player_id());
///<<< END WRITING YOUR CODE 
}

///<<<rpc end
