#ifndef SRC_GATEWAY_SERVICE_SERVICE_H_
#define SRC_GATEWAY_SERVICE_SERVICE_H_

#include "src/server_rpc_client/rpc_string_closure.h"
#include "src/redis_client/redis_client.h"

#include "gw2l.pb.h"

namespace gw2l
{
    using common::RedisClientPtr;
    class LoginServiceImpl : public LoginService
    {
    public:
        using MessagePtr = std::unique_ptr<google::protobuf::Message>;
       
        virtual void Login(::google::protobuf::RpcController* controller,
            const gw2l::LoginRequest* request,
            gw2l::LoginResponse* response,
            ::google::protobuf::Closure* done)override;

        virtual void CratePlayer(::google::protobuf::RpcController* controller,
            const gw2l::CreatePlayerRequest* request,
            gw2l::CreatePlayerRespone* response,
            ::google::protobuf::Closure* done)override;

        virtual void EnterGame(::google::protobuf::RpcController* controller,
            const ::gw2l::EnterGameRequest* request,
            ::gw2l::EnterGameRequest* response,
            ::google::protobuf::Closure* done)override;

        void set_redis_client(RedisClientPtr& p)
        {
            redis_ = p;
        }
    private:
        RedisClientPtr redis_;
    };

}  // namespace gw2l

#endif // SRC_GATEWAY_SERVICE_SERVICE_H_