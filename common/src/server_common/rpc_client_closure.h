#ifndef COMMON_SRC_server_common_RPC_CLIENT_CLOSURE_H_
#define COMMON_SRC_server_common_RPC_CLIENT_CLOSURE_H_

namespace common
{
    template <typename ClientResponse, typename ServerRequest, typename ServerResponse>
    struct ClientClosure
    {
        ClientClosure(const muduo::net::TcpConnectionPtr& cc)
            : s_resp_(new ServerResponse()),
              client_connection_(cc)
        {
        }

        uint64_t connection_hash_id() {
            return uint64_t(client_connection_.get());
        }

        ~ClientClosure() {};
        ClientResponse c_resp_;
        ServerRequest s_reqst_;
        ServerResponse* s_resp_{ nullptr };
        const muduo::net::TcpConnectionPtr client_connection_;
    };

}//namespace common

#endif // COMMON_SRC_server_common_RPC_CLIENT_CLOSURE_H_