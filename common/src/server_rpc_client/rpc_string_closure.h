#ifndef COMMON_SRC_SERVER_CRPC_CLIENT_RPC_STRING_CLOSURE_H_
#define COMMON_SRC_SERVER_CRPC_CLIENT_RPC_STRING_CLOSURE_H_

#include "google/protobuf/message.h"

namespace common
{
    template <typename ServerRequest, typename ServerRespone, typename ClientRespone>
    struct RpcString
    {
        RpcString(ClientRespone* client_respone,
            ::google::protobuf::Closure* client_closure)
            : client_respone_(client_respone),
            client_closure_(client_closure),
            server_respone_(new ServerRespone())// delete for rpcchanel
            {}

        ~RpcString() { client_closure_->Run(); };//this function delete server_respone_
        ClientRespone* client_respone_{ nullptr };
        ServerRequest server_request_;
        ServerRespone* server_respone_{ nullptr };  
        ::google::protobuf::Closure* client_closure_{ nullptr };
    };

}//namespace common
#endif //COMMON_SRC_SERVER_CRPC_CLIENT_RPC_STRING_CLOSURE_H_