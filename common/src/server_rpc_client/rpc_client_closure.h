#ifndef COMMON_SRC_SERVER_CRPC_CLIENT_RPC_CLIENT_CLOSURE_H_
#define COMMON_SRC_SERVER_CRPC_CLIENT_RPC_CLIENT_CLOSURE_H_

namespace common
{
    template <typename ClientRespone, typename ServerRequest, typename ServerRespone>
    struct ClientClosureParam
    {
        ClientClosureParam(const muduo::net::TcpConnectionPtr& cc)
            : server_respone_(new ServerRespone()),
              client_connection_(cc){}

        ~ClientClosureParam() {};
        ClientRespone client_respone_;
        ServerRequest server_request_;
        ServerRespone* server_respone_{ nullptr };
        const muduo::net::TcpConnectionPtr client_connection_;
    };

}//namespace common

#endif // COMMON_SRC_SERVER_CRPC_CLIENT_RPC_CLIENT_CLOSURE_H_