#ifndef COMMON_SRC_SERVER_RPC_CLIENT_RPC_CLOSURE_H_
#define COMMON_SRC_SERVER_RPC_CLIENT_RPC_CLOSURE_H_

#include <boost/any.hpp>

#include "google/protobuf/message.h"

#include "muduo/net/TcpConnection.h"

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

	template <typename ClientResponse, typename ServerRequest, typename ServerResponse>
	struct ClosureReplied
	{
		ClosureReplied(const muduo::net::TcpConnectionPtr& cc)
			: s_rp_(new ServerResponse()),
			client_conn_(cc) {}

		inline uint64_t conn_id() const { return  uint64_t(client_conn_.get()); }
		ClientResponse c_rp_;
		ServerRequest s_rq_;
		ServerResponse* s_rp_{ nullptr };
		const muduo::net::TcpConnectionPtr client_conn_;
	};

	template <typename ServerRequest, typename ServerResponse, typename ClientResponse>
	struct RpcString
	{
		RpcString(ClientResponse* client_response,
			::google::protobuf::Closure* client_closure)
			: c_rp_(client_response),
			s_rp_(new ServerResponse()),// delete for rpcchanel
			cc_(client_closure)
		{}

		template <typename MoveRpcString>
		RpcString(MoveRpcString& rpcstring)
			: s_rp_(new ServerResponse())// delete for rpcchanel
		{
			rpcstring.Move(c_rp_, cc_);
		}

		~RpcString() { if (nullptr != cc_) { cc_->Run(); } };//this function delete server_response_ if not move
		ClientResponse* c_rp_{ nullptr };
		ServerRequest s_rq_;
		ServerResponse* s_rp_{ nullptr };

		//just for enter master , un safe
		void Move(ClientResponse*& c_rp,
			::google::protobuf::Closure*& cc)
		{
			c_rp = c_rp_;
			cc = cc_;
			cc_ = nullptr;
		}
	private:
		::google::protobuf::Closure* cc_{ nullptr };
	};

}//namespace common

#endif //COMMON_SRC_SERVER_RPC_CLIENT_RPC_CLOSURE_H_