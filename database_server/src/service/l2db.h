#ifndef DATABASE_SERVER_SRC_SERVICE_H_
#define DATABASE_SERVER_SRC_SERVICE_H_
#include "l2db.pb.h"
#include "l2db.pb.h"
///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"
#include "src/server_common/rpc_server.h"
#include "src/mysql_database/mysql_database.h"

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
namespace l2db{
    ///<<< BEGIN WRITING YOUR CODE
    ///<<< END WRITING YOUR CODE
class LoginServiceImpl : public LoginService{
public:
    void Login(::google::protobuf::RpcController* controller,
        const l2db::LoginRequest* request,
        l2db::LoginResponse* response,
        ::google::protobuf::Closure* done)override;

    void CreatePlayer(::google::protobuf::RpcController* controller,
        const l2db::CreatePlayerRequest* request,
        l2db::CreatePlayerResponse* response,
        ::google::protobuf::Closure* done)override;

    void EnterGame(::google::protobuf::RpcController* controller,
        const l2db::EnterGameRequest* request,
        l2db::EnterGameResponse* response,
        ::google::protobuf::Closure* done)override;

        ///<<< BEGIN WRITING YOUR CODE
		using MysqlClientPtr = std::shared_ptr<common::MysqlDatabase>;
		using RedisClientPtr = std::shared_ptr<common::RedisClient>;

		static LoginServiceImpl& GetSingleton()
		{
			thread_local LoginServiceImpl singleton;
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
};
}// namespace l2db
#endif//DATABASE_SERVER_SRC_SERVICE_H_
