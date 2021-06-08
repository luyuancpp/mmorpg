#ifndef SRC_LOGIN_SERVICE_SERVICE_H_
#define SRC_LOGIN_SERVICE_SERVICE_H_

#ifdef __linux__
#include <unistd.h>
#endif//__linux__

#include "l2db.pb.h"

#include "muduo/base/Logging.h"
#include "muduo/net/protorpc/RpcServer.h"

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

namespace l2db
{
    
    class LoginServiceImpl : public LoginService
    {
    public:
        using MysqlClientPtr = std::shared_ptr<database::MysqlDatabase>;
        using RedisClientPtr = std::shared_ptr<common::RedisClient>;
        virtual void Login(::google::protobuf::RpcController* controller,
            const l2db::LoginRequest* request,
            l2db::LoginResponse* response,
            ::google::protobuf::Closure* done)override;

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
    };

}  // namespace l2db

#endif // SRC_LOGIN_SERVICE_SERVICE_H_