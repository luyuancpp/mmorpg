#ifndef SRC_GATEWAY_SERVICE_SERVICE_H_
#define SRC_GATEWAY_SERVICE_SERVICE_H_

#include "src/server_rpc_client/rpc_string_closure.h"

#include "gw2l.pb.h"

namespace gw2l
{

    class LoginServiceImpl : public LoginService
    {
    public:
        using MessagePtr = std::unique_ptr<google::protobuf::Message>;
        
        virtual void Login(::google::protobuf::RpcController* controller,
            const gw2l::LoginRequest* request,
            gw2l::LoginResponse* response,
            ::google::protobuf::Closure* done)override;

    };

}  // namespace gw2l

#endif // SRC_GATEWAY_SERVICE_SERVICE_H_