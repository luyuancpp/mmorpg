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
            : c_resp_(client_respone),
            cc_(client_closure),
            s_resp_(new ServerRespone())// delete for rpcchanel
            {}

        ~RpcString() { cc_->Run(); };//this function delete server_respone_
        ClientRespone* c_resp_{ nullptr };
        ServerRequest s_reqst_;
        ServerRespone* s_resp_{ nullptr };  
        ::google::protobuf::Closure* cc_{ nullptr };
    };

}//namespace common
#endif //COMMON_SRC_SERVER_CRPC_CLIENT_RPC_STRING_CLOSURE_H_