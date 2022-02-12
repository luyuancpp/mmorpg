#ifndef COMMON_SRC_SERVER_RPC_CLIENT_RPC_CLOSURE_H_
#define COMMON_SRC_SERVER_RPC_CLIENT_RPC_CLOSURE_H_

namespace common
{
template <typename ServerRequest, typename ServerResponse>
struct RpcClosure
{
    RpcClosure()
        : s_rp_(new ServerResponse())// delete for rpcchanel
    {}

    ServerRequest s_rq_;
    ServerResponse* s_rp_{ nullptr };
private:

};
}//namespace common

#endif //COMMON_SRC_SERVER_RPC_CLIENT_RPC_CLOSURE_H_