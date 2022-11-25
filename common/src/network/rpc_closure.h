#pragma once

#include <boost/any.hpp>

#include "google/protobuf/message.h"
#include "google/protobuf/stubs/callback.h"

#include "muduo/net/TcpConnection.h"



template <typename ServerRequest, typename ServerResponse, typename ClientResponse>
struct RpcString
{
	RpcString(ClientResponse* client_response,
		::google::protobuf::Closure* client_closure)
		: c_rp_(client_response),
		s_rp_(new ServerResponse()),// delete for rpcchanel
		cc_(client_closure)
	{}

	RpcString(const RpcString&) = delete;
	RpcString& operator = (const RpcString&) = delete;

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

	//just for enter controller , un safe
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

class AutoRecycleClosure
{
public:
	using element_type = ::google::protobuf::Closure;
	AutoRecycleClosure(element_type* done) : done_(done) {}
	~AutoRecycleClosure() { if (nullptr != done_) { done_->Run(); } }
	void SelfDelete() { done_ = nullptr; }//如果不自动回收的话就调用这个接口,让出管理权
private:
	element_type* done_{ nullptr };
};
