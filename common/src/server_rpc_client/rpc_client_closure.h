#ifndef COMMON_SRC_SERVER_CRPC_CLIENT_RPC_CLIENT_CLOSURE_H_
#define COMMON_SRC_SERVER_CRPC_CLIENT_RPC_CLIENT_CLOSURE_H_

namespace common
{
    template <typename ClientRespone, typename ServerRequest, typename ServerRespone>
    struct ClientClosure
    {
        ClientClosure(const muduo::net::TcpConnectionPtr& cc)
            : s_resp_(new ServerRespone()),
              client_connection_(cc){}

        ~ClientClosure() {};
        ClientRespone c_resp_;
        ServerRequest server_request_;
        ServerRespone* s_resp_{ nullptr };
        const muduo::net::TcpConnectionPtr client_connection_;
    };

}//namespace common

#endif // COMMON_SRC_SERVER_CRPC_CLIENT_RPC_CLIENT_CLOSURE_H_