#ifndef SRC_GATEWAY_SERVICE_SERVICE_H_
#define SRC_GATEWAY_SERVICE_SERVICE_H_

#ifdef __linux__
#include <unistd.h>
#endif//__linux__

#include "gw2l.pb.h"

namespace gw2l
{
    template <typename ServerRequest, typename ServerRespone, typename ClientRespone>
    struct ClosureParam
    {
        ClosureParam(ClientRespone* client_respone,
            ::google::protobuf::Closure* client_closure)
            : server_respone_(new ServerRespone()),
            client_closure_(client_closure),
            client_respone_(client_respone)
        {

        }
        ~ClosureParam() { client_closure_->Run(); };
        ServerRespone* server_respone_{ nullptr };
        ::google::protobuf::Closure* client_closure_{ nullptr };
        ClientRespone* client_respone_{ nullptr };
        ServerRequest server_request_;
    };

    class LoginServiceImpl : public LoginService
    {
    public:
        using MessagePtr = std::unique_ptr<google::protobuf::Message>;
        
        virtual void Login(::google::protobuf::RpcController* controller,
            const gw2l::LoginRequest* request,
            gw2l::LoginResponse* response,
            ::google::protobuf::Closure* done)override;
       
        template <typename ServerRespone, typename CClosure, typename ClientRespone>
        void DbLoginReplied(ClosureParam<ServerRespone, CClosure, ClientRespone>* respone) 
        {
            std::unique_ptr<ClosureParam<ServerRespone, CClosure, ClientRespone>> d(respone);
        }
    };

}  // namespace gw2l

#endif // SRC_GATEWAY_SERVICE_SERVICE_H_