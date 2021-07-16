#ifndef COMMON_SRC_SERVER_RPC_CLIENT_RPC_CLOSURE_H_
#define COMMON_SRC_SERVER_RPC_CLIENT_RPC_CLOSURE_H_

namespace common
{
template <typename ServerRequest, typename ServerResponse>
struct RpcClosure
{
    RpcClosure()
        : s_resp_(new ServerResponse())// delete for rpcchanel
    {}

    ServerRequest s_reqst_;
    ServerResponse* s_resp_{ nullptr };
private:

};
}//namespace common

#endif //COMMON_SRC_SERVER_RPC_CLIENT_RPC_CLOSURE_H_