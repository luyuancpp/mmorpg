#pragma once

#include <boost/any.hpp>

#include "google/protobuf/message.h"
#include "google/protobuf/stubs/callback.h"

#include "muduo/net/TcpConnection.h"

#include "common_proto/node_common.pb.h"


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


struct ThreadLocalRoteMsgBodyList
{

};




extern thread_local ThreadLocalRoteMsgBodyList g_route_msg;

extern const int32_t kMaxRouteSize;
extern RouteInfo route2db;
extern RouteInfo route2controller;
extern RouteInfo route2gate;
extern std::string route_msg_body;