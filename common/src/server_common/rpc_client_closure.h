#ifndef COMMON_SRC_server_common_RPC_CLIENT_CLOSURE_H_
#define COMMON_SRC_server_common_RPC_CLIENT_CLOSURE_H_

#include "muduo/net/TcpConnection.h"

namespace common
{
    template <typename ClientResponse, typename ServerRequest, typename ServerResponse>
    struct ServerReplied
    {
        ServerReplied(const muduo::net::TcpConnectionPtr& cc)
            : s_rp_(new ServerResponse()),
              client_conn_(cc){}

        inline uint64_t conn_id() const{ return  uint64_t(client_conn_.get()); }
        ClientResponse c_rp_;
        ServerRequest s_rq_;
        ServerResponse* s_rp_{ nullptr };
        const muduo::net::TcpConnectionPtr client_conn_;
    };

	

}//namespace common

#endif // COMMON_SRC_server_common_RPC_CLIENT_CLOSURE_H_