#ifndef COMMON_SRC_RPC_CLOSURE_PARAM_RPC_CLIENT_CLOSURE_H_
#define COMMON_SRC_RPC_CLOSURE_PARAM_RPC_CLIENT_CLOSURE_H_

#include <any>

namespace common
{
    template <typename ClientRespone, typename ServerRequest, typename ServerRespone>
    struct ClientClosure
    {
        ClientClosure(const muduo::net::TcpConnectionPtr& cc)
            : s_resp_(new ServerRespone()),
              client_connection_(cc)
        {
            uint64_t p = (uint64_t)(client_connection_.get());
            boost::any connection_context = p;
            client_connection_->setContext(connection_context);
        }

        uint64_t connection_hash_id() {
            return boost::any_cast<uint64_t>(client_connection_->getContext());
        }

        ~ClientClosure() {};
        ClientRespone c_resp_;
        ServerRequest s_rqst_;
        ServerRespone* s_resp_{ nullptr };
        const muduo::net::TcpConnectionPtr client_connection_;
    };

}//namespace common

#endif // COMMON_SRC_RPC_CLOSURE_PARAM_RPC_CLIENT_CLOSURE_H_