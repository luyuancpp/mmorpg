#ifndef DATABASE_SERVER_SRC_SERVICE_DB_NODE_H_
#define DATABASE_SERVER_SRC_SERVICE_DB_NODE_H_
#include "db_node.pb.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/network/rpc_server.h"
#include "src/mysql_wrapper/mysql_database.h"

using namespace muduo;
using namespace muduo::net;

namespace database
{
	class MysqlDatabase;
}//namespace database;

namespace common
{
	class RedisClient;
}//namespace common;

///<<< END WRITING YOUR CODE
namespace dbservice{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class DbServiceImpl : public DbService{
public:
///<<< BEGIN WRITING YOUR CODE
	using MysqlClientPtr = std::shared_ptr<common::MysqlDatabase>;
	using RedisClientPtr = std::shared_ptr<common::RedisClient>;

	static DbServiceImpl& GetSingleton()
	{
		thread_local DbServiceImpl singleton;
		return singleton;
	}

	void set_player_mysql_client(MysqlClientPtr& ptr)
	{
		database_ = ptr;
	}

	void set_redis_client(RedisClientPtr& ptr)
	{
		redis_ = ptr;
	}
private:
	MysqlClientPtr database_;
	RedisClientPtr redis_;
///<<< END WRITING YOUR CODE
public:
    void Login(::google::protobuf::RpcController* controller,
        const dbservice::LoginRequest* request,
        dbservice::LoginResponse* response,
        ::google::protobuf::Closure* done)override;

    void CreatePlayer(::google::protobuf::RpcController* controller,
        const dbservice::CreatePlayerRequest* request,
        dbservice::CreatePlayerResponse* response,
        ::google::protobuf::Closure* done)override;

    void EnterGame(::google::protobuf::RpcController* controller,
        const dbservice::EnterGameRequest* request,
        dbservice::EnterGameResponse* response,
        ::google::protobuf::Closure* done)override;

};
}// namespace dbservice
#endif//DATABASE_SERVER_SRC_SERVICE_DB_NODE_H_
