#ifndef COMMON_SRC_SERVER_RPC_CLIENT_RPC_STRING_CLOSURE_H_
#define COMMON_SRC_SERVER_RPC_CLIENT_RPC_STRING_CLOSURE_H_

#include "google/protobuf/message.h"

#include <boost/any.hpp>

namespace common
{
    template <typename ServerRequest, typename ServerResponse, typename ClientResponse>
    struct RpcString
    {
        RpcString(ClientResponse* client_response,
            ::google::protobuf::Closure* client_closure)
            : c_resp_(client_response),
            s_resp_(new ServerResponse()),// delete for rpcchanel
            cc_(client_closure)
            {}

        template <typename MoveRpcString>
        RpcString(MoveRpcString& rpcstring)
            : s_resp_(new ServerResponse())// delete for rpcchanel
        {
            rpcstring.Move(c_resp_, cc_);
        }

        ~RpcString() { if (nullptr != cc_) { cc_->Run(); } };//this function delete server_response_ if not move
        ClientResponse* c_resp_{ nullptr };
        ServerRequest s_reqst_;
        ServerResponse* s_resp_{ nullptr }; 

        //just for enter master , un safe
        void Move(ClientResponse*& client_response,
            ::google::protobuf::Closure*& client_closure)
        {
            client_response = c_resp_;
            client_closure = cc_;
            cc_ = nullptr;
        }
    private:
        ::google::protobuf::Closure* cc_{ nullptr };
    };

}//namespace common
#endif //COMMON_SRC_SERVER_RPC_CLIENT_RPC_STRING_CLOSURE_H_
