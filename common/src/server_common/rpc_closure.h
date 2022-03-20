#ifndef COMMON_SRC_SERVER_RPC_CLIENT_RPC_CLOSURE_H_
#define COMMON_SRC_SERVER_RPC_CLIENT_RPC_CLOSURE_H_

namespace common
{
	template <typename Request, typename Response>
	struct NormalClosure
	{
		NormalClosure()
			: s_rp_(new Response())//delete from rpc_chanel respone
		{}
		Request s_rq_;
		Response* s_rp_{ nullptr };
	};
}//namespace common

#endif //COMMON_SRC_SERVER_RPC_CLIENT_RPC_CLOSURE_H_