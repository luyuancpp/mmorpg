// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <cstdint>

#include "rpc_channel.h"

#include "muduo/base/Logging.h"

#include <google/protobuf/descriptor.h>

#include "network/message_statistics.h"
#include "service_info/service_info.h"
#include "test/test.h"

#include "proto/common/empty.pb.h"

using namespace muduo;
using namespace muduo::net;

void OnUnknownMessage(const TcpConnectionPtr&,
    const MessagePtr& message,
    Timestamp)
{

}
ProtobufDispatcher g_response_dispatcher(std::bind(&OnUnknownMessage,  _1, _2, _3));

RpcChannel::RpcChannel()
	: codec_(std::bind(&RpcChannel::onRpcMessage, this, _1, _2, _3)),
	services_(NULL),
	dispatcher_(std::bind(&RpcChannel::onUnknownMessage, this, _1, _2, _3))
{
	LOG_DEBUG << "RpcChannel::ctor - " << this;
}

RpcChannel::RpcChannel(const TcpConnectionPtr& conn)
	: codec_(std::bind(&RpcChannel::onRpcMessage, this, _1, _2, _3)),
	conn_(conn),
	services_(NULL),
	dispatcher_(std::bind(&RpcChannel::onUnknownMessage, this, _1, _2, _3))
{
	LOG_DEBUG << "RpcChannel::ctor - " << this;
}

RpcChannel::~RpcChannel()
{
	LOG_DEBUG << "RpcChannel::dtor - " << this;

}

  // Call the given method of the remote service.  The signature of this
  // procedure looks the same as Service::CallMethod(), but the requirements
  // are less strict in one important way:  the request and response objects
  // need not be of any specific class as long as their descriptors are
  // method->input_type() and method->output_type().
void RpcChannel::CallMethod(uint32_t message_id, const ::google::protobuf::Message& request)
{
  RpcMessage message;
  message.set_type(REQUEST);
  message.set_message_id(message_id);
  auto byte_size = int32_t(request.ByteSizeLong());
  message.mutable_request()->resize(byte_size);
  // FIXME: error check
  if (!request.SerializePartialToArray(message.mutable_request()->data(), byte_size))
  {
	  LOG_ERROR << "message error " << this;
	  return;
  }
  SendMessage(message);
}

void RpcChannel::Send(uint32_t message_id, const ::google::protobuf::Message& request)
{
	if (message_id >= g_message_info.size())
	{
		LOG_ERROR << "message_id found ->" << message_id;
		return;
	}

    RpcMessage message;    
    message.set_type(S2C_REQUEST);
    message.set_message_id(message_id);  
	auto byte_size = int32_t(request.ByteSizeLong());
    message.mutable_request()->resize(byte_size);
    // FIXME: error check
    if (!request.SerializePartialToArray(message.mutable_request()->data(), byte_size))
    {
        LOG_ERROR << "message error " << this;
        return;
    }
	SendMessage(message);
}

void RpcChannel::onMessage(const TcpConnectionPtr& conn,
                           Buffer* buf,
                           Timestamp receiveTime)
{
  codec_.onMessage(conn, buf, receiveTime);
}

void RpcChannel::onUnknownMessage(const TcpConnectionPtr&, const MessagePtr& message, Timestamp)
{
    LOG_ERROR << "onUnknownMessage: " << message->GetTypeName();
}

void RpcChannel::onRpcMessage(const TcpConnectionPtr& conn,
                              const RpcMessagePtr& messagePtr,
                              Timestamp receiveTime)
{
  assert(conn == conn_);

  RpcMessage& message = *messagePtr;
  if (message.type() == RESPONSE)
  {
      if ( message.message_id() >= g_message_info.size())
      {
          return;
      }
	  auto& message_info = g_message_info[message.message_id()];
	  if (nullptr == message_info.service_impl_instance_)
	  {
		  return;
	  }
	  google::protobuf::Service* service = message_info.service_impl_instance_.get();
	  const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
	  const google::protobuf::MethodDescriptor* method = desc->FindMethodByName(message_info.method);
	  if (nullptr == method)
	  {
		  return;
	  }
	  const MessagePtr response(service->GetResponsePrototype(method).New());
	  if (!response->ParsePartialFromArray(message.response().data(), int32_t(message.response().size())))
	  {
		  LOG_ERROR << "ParsePartialFromArray error";
		  return;
	  }
	  g_response_dispatcher.onProtobufMessage(conn, response, receiveTime);
  }
  else if (message.type() == REQUEST)
  {
      onNormalRequestResponseMessage(conn, message, receiveTime);
  }
  else if (message.type() == S2C_REQUEST)
  {
      onS2CMessage(conn, message, receiveTime);
  }
  else if (message.type() == NODE_ROUTE)
  {
    onRouteNodeMessage(conn, message, receiveTime);
  }
  else if (message.type() == RPC_ERROR)
  {
  }
}

void RpcChannel::Route2Node(uint32_t message_id, const ::google::protobuf::Message& request)
{
    RpcMessage message;
    message.set_type(NODE_ROUTE);  
	auto byte_size = int32_t(request.ByteSizeLong());
    message.mutable_request()->resize(byte_size);
    // FIXME: error check
    if (!request.SerializePartialToArray(message.mutable_request()->data(), byte_size))
    {
        LOG_ERROR << "message error " << this;
        return;
    }
    message.set_message_id(message_id);
	SendMessage(message);
}

void RpcChannel::onRouteNodeMessage(const TcpConnectionPtr& conn, const RpcMessage& message, Timestamp receiveTime)
{
	assert(services_ != NULL);
	if ( message.message_id() >= g_message_info.size())
	{
		return;
	}
	auto& message_info = g_message_info[message.message_id()];
	std::map<std::string, google::protobuf::Service*>::const_iterator it = services_->find(message_info.service);
	if (it == services_->end())
	{
		SendRpcError(message, NO_SERVICE);
		return;
	}
	google::protobuf::Service* service = it->second;
	assert(service != NULL);
	const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
	const google::protobuf::MethodDescriptor* method = desc->FindMethodByName(message_info.method);
	if (nullptr == method)
	{
		SendRpcError(message, NO_METHOD);
		return;
	}
    std::unique_ptr<google::protobuf::Message> request(service->GetRequestPrototype(method).New());
    if (!request->ParsePartialFromArray(message.request().data(), int32_t(message.request().size() )))
    {
		LOG_ERROR << "ParsePartialFromArray error";
        SendRpcError(message, INVALID_REQUEST);
        return;
    }
    std::unique_ptr<google::protobuf::Message> response(service->GetResponsePrototype(method).New());
    service->CallMethod(method, nullptr, get_pointer(request), get_pointer(response), nullptr);
    if (response->ByteSizeLong() <= 0)
    {
        return;
    }
    RpcMessage rpc_response;
    rpc_response.set_type(RESPONSE);
    auto byte_size = int32_t(response->ByteSizeLong());
	rpc_response.mutable_response()->resize(byte_size);
    // FIXME: error check
    if (!response->SerializePartialToArray(rpc_response.mutable_response()->data(), byte_size))
    {
        LOG_ERROR << "message error " << this;
        return;
    }
    rpc_response.set_message_id(message.message_id());
	SendMessage(rpc_response);
}

void RpcChannel::onS2CMessage(const TcpConnectionPtr& conn, const RpcMessage& message, Timestamp receiveTime)
{
	if ( message.message_id() >= g_message_info.size())
	{
		return;
	}
	auto& message_info = g_message_info[message.message_id()];
	std::map<std::string, google::protobuf::Service*>::const_iterator it = services_->find(message_info.service);
	if (it == services_->end())
	{
		SendRpcError(message, NO_SERVICE);
		return;
	}
	google::protobuf::Service* service = it->second;
	const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
	const google::protobuf::MethodDescriptor* method = desc->FindMethodByName(message_info.method);
	if (nullptr == method)
	{
		SendRpcError(message, NO_METHOD);
		return;
	}
    std::unique_ptr<google::protobuf::Message> request(service->GetRequestPrototype(method).New());
    if (!request->ParsePartialFromArray(message.request().data(), int32_t(message.request().size())))
    {
		LOG_ERROR << "ParsePartialFromArray error";
        SendRpcError(message, INVALID_REQUEST);
        return;
    }
    service->CallMethod(method, nullptr, get_pointer(request), nullptr, nullptr);
}

void RpcChannel::onNormalRequestResponseMessage(const TcpConnectionPtr& conn, const RpcMessage& message, Timestamp receiveTime)
{
	if ( message.message_id() >= g_message_info.size())
	{
		return;
	}
	auto& message_info = g_message_info[message.message_id()];
	std::map<std::string, google::protobuf::Service*>::const_iterator it = services_->find(message_info.service);
	if (it == services_->end())
	{
		SendRpcError(message, NO_SERVICE);
		return;
	}
	google::protobuf::Service* service = it->second;
	const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
	const google::protobuf::MethodDescriptor* method = desc->FindMethodByName(message_info.method);
	if (nullptr == method)
	{
		SendRpcError(message, NO_METHOD);
		return;
	}
    std::unique_ptr<google::protobuf::Message> request(service->GetRequestPrototype(method).New());
    if (!request->ParsePartialFromArray(message.request().data(), int32_t(message.request().size())))
    {
		LOG_ERROR << "ParsePartialFromArray error";
        SendRpcError(message, INVALID_REQUEST);
        return;
    }
    if (service->GetResponsePrototype(method).GetDescriptor() == Empty::GetDescriptor())
    {
        service->CallMethod(method, NULL, get_pointer(request), nullptr, nullptr);
    }
    else
    {
        std::unique_ptr<google::protobuf::Message> response(service->GetResponsePrototype(method).New());
        service->CallMethod(method, NULL, get_pointer(request), get_pointer(response), nullptr);
        RpcMessage rpc_response;
        rpc_response.set_type(RESPONSE);
        auto byte_size = int32_t(response->ByteSizeLong());
		rpc_response.mutable_response()->resize(byte_size);
        // FIXME: error check
        if (!response->SerializePartialToArray(rpc_response.mutable_response()->data(), byte_size))
        {
            LOG_ERROR << "message error " << this;
            return;
        }
        rpc_response.set_message_id(message.message_id());
		SendMessage(rpc_response);
    }
}

void RpcChannel::SendRpcError(const RpcMessage& message, ErrorCode error)
{
    RpcMessage response;
    response.set_type(RESPONSE);
    response.set_error(error);
	SendMessage(response);
}

void RpcChannel::SendRouteResponse(uint32_t message_id, uint64_t id, const std::string& body)
{
	if (message_id >= g_message_info.size())
	{
		LOG_ERROR << "message_id found ->" << message_id;
		return;
	}
    //todo check message id error
    RpcMessage response;
    response.set_type(RESPONSE);
    response.set_response(body); // FIXME: error check
    response.set_message_id(message_id);
	SendMessage(response);
}

void RpcChannel::MessageStatistics(const RpcMessage& message)
{
	if (!g_test_switch_list[kMessageStatistics])
	{
		return;
	}
	auto& statistic = g_message_statistics[message.message_id()];
	statistic.set_count(statistic.count() + 1);
}

void RpcChannel::SendMessage(const RpcMessage& message)
{
	codec_.send(conn_, message);

	MessageStatistics(message);
}

