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
#include <google/protobuf/empty.pb.h>

#include "src/pb/pbc/service.h"


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
	id_(0),
	services_(NULL),
	dispatcher_(std::bind(&RpcChannel::onUnknownMessage, this, _1, _2, _3))
{
	LOG_INFO << "RpcChannel::ctor - " << this;
}

RpcChannel::RpcChannel(const TcpConnectionPtr& conn)
	: codec_(std::bind(&RpcChannel::onRpcMessage, this, _1, _2, _3)),
	conn_(conn),
	id_(0),
	services_(NULL),
	dispatcher_(std::bind(&RpcChannel::onUnknownMessage, this, _1, _2, _3))
{
	LOG_INFO << "RpcChannel::ctor - " << this;
}

RpcChannel::~RpcChannel()
{
  LOG_INFO << "RpcChannel::dtor - " << this;

}

  // Call the given method of the remote service.  The signature of this
  // procedure looks the same as Service::CallMethod(), but the requirements
  // are less strict in one important way:  the request and response objects
  // need not be of any specific class as long as their descriptors are
  // method->input_type() and method->output_type().
void RpcChannel::CallMethod(const ::google::protobuf::MethodDescriptor* method,
                            google::protobuf::RpcController* controller,
                            const ::google::protobuf::Message* request,
                            ::google::protobuf::Message* response,
                            ::google::protobuf::Closure* done)
{
  RpcMessage message;
  message.set_type(REQUEST);
  int64_t id = ++id_;
  message.set_id(id);
  //message.set_service(method->service()->full_name());
  //message.set_method(method->name());
  message.set_request(request->SerializeAsString()); // FIXME: error check
  codec_.send(conn_, message);
}

void RpcChannel::Send(uint32_t message_id, const ::google::protobuf::Message& request)
{
    RpcMessage message;    
    message.set_type(S2C_REQUEST);
    message.set_message_id(message_id);
    message.set_request(request.SerializeAsString()); // FIXME: error check
    codec_.send(conn_, message);
}

void RpcChannel::onMessage(const TcpConnectionPtr& conn,
                           Buffer* buf,
                           Timestamp receiveTime)
{
  codec_.onMessage(conn, buf, receiveTime);
}

void RpcChannel::onUnknownMessage(const TcpConnectionPtr&, const MessagePtr& message, Timestamp)
{
    LOG_INFO << "onUnknownMessage: " << message->GetTypeName();
}

void RpcChannel::onRpcMessage(const TcpConnectionPtr& conn,
                              const RpcMessagePtr& messagePtr,
                              Timestamp receiveTime)
{
  assert(conn == conn_);

  RpcMessage& message = *messagePtr;
  if (message.type() == RESPONSE)
  {
	  assert(services_ != NULL);
	  auto message_it = g_services.find(message.message_id());
	  if (message_it == g_services.end())
	  {
		  return;
	  }
	  std::map<std::string, google::protobuf::Service*>::const_iterator it = services_->find(message_it->second.service);
	  if (nullptr == message_it->second.service_impl_instance_)
	  {
		  return;
	  }
	  google::protobuf::Service* service = message_it->second.service_impl_instance_.get();
	  assert(service != NULL);
	  const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
	  const google::protobuf::MethodDescriptor* method = desc->FindMethodByName(message_it->second.method);
	  if (nullptr == method)
	  {
		  return;
	  }
	  MessagePtr response(service->GetResponsePrototype(method).New());
	  response->ParseFromString(message.response());
	  g_response_dispatcher.onProtobufMessage(conn, response, receiveTime);
  }
  else if (message.type() == REQUEST)
  {
    // FIXME: extract to a function
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
    message.set_request(request.SerializeAsString()); // FIXME: error check
    message.set_message_id(message_id);
    codec_.send(conn_, message);
}

void RpcChannel::onRouteNodeMessage(const TcpConnectionPtr& conn, const RpcMessage& message, Timestamp receiveTime)
{
	assert(services_ != NULL);
	auto message_it = g_services.find(message.message_id());
	if (message_it == g_services.end())
	{
		SendRpcError(message, NO_SERVICE);
		return;
	}
	std::map<std::string, google::protobuf::Service*>::const_iterator it = services_->find(message_it->second.service);
	if (it == services_->end())
	{
		SendRpcError(message, NO_SERVICE);
		return;
	}
	google::protobuf::Service* service = it->second;
	assert(service != NULL);
	const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
	const google::protobuf::MethodDescriptor* method = desc->FindMethodByName(message_it->second.method);
	if (nullptr == method)
	{
		SendRpcError(message, NO_METHOD);
		return;
	}
    std::unique_ptr<google::protobuf::Message> request(service->GetRequestPrototype(method).New());
    if (!request->ParseFromString(message.request()))
    {
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
    rpc_response.set_id(message.id());
    rpc_response.set_response(response->SerializeAsString()); // FIXME: error check
    rpc_response.set_message_id(message.message_id());
    codec_.send(conn_, rpc_response);
}

void RpcChannel::onS2CMessage(const TcpConnectionPtr& conn, const RpcMessage& message, Timestamp receiveTime)
{
	assert(services_ != NULL);
	auto message_it = g_services.find(message.message_id());
	if (message_it == g_services.end())
	{
		SendRpcError(message, NO_SERVICE);
		return;
	}
	std::map<std::string, google::protobuf::Service*>::const_iterator it = services_->find(message_it->second.service);
	if (it == services_->end())
	{
		SendRpcError(message, NO_SERVICE);
		return;
	}
	google::protobuf::Service* service = it->second;
	assert(service != NULL);
	const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
	const google::protobuf::MethodDescriptor* method = desc->FindMethodByName(message_it->second.method);
	if (nullptr == method)
	{
		SendRpcError(message, NO_METHOD);
		return;
	}
    std::unique_ptr<google::protobuf::Message> request(service->GetRequestPrototype(method).New());
    if (!request->ParseFromString(message.request()))
    {
        SendRpcError(message, INVALID_REQUEST);
        return;
    }
    service->CallMethod(method, nullptr, get_pointer(request), nullptr, nullptr);
}

void RpcChannel::onNormalRequestResponseMessage(const TcpConnectionPtr& conn, const RpcMessage& message, Timestamp receiveTime)
{
    assert(services_ != NULL);
    auto message_it = g_services.find(message.message_id());
    if (message_it == g_services.end())
    {
        SendRpcError(message, NO_SERVICE);
        return;
    }
	std::map<std::string, google::protobuf::Service*>::const_iterator it = services_->find(message_it->second.service);
	if (it == services_->end())
	{
		SendRpcError(message, NO_SERVICE);
		return;
	}
    google::protobuf::Service* service = it->second;
    assert(service != NULL);
    const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
    const google::protobuf::MethodDescriptor* method = desc->FindMethodByName(message_it->second.method);
    if (nullptr ==  method)
    {
        SendRpcError(message, NO_METHOD);
        return;
    }
    std::unique_ptr<google::protobuf::Message> request(service->GetRequestPrototype(method).New());
    if (!request->ParseFromString(message.request()))
    {
        SendRpcError(message, INVALID_REQUEST);
        return;
    }
    if (service->GetResponsePrototype(method).GetDescriptor() == ::google::protobuf::Empty::GetDescriptor())
    {
        service->CallMethod(method, NULL, get_pointer(request), nullptr, nullptr);
    }
    else
    {
        std::unique_ptr<google::protobuf::Message> response(service->GetResponsePrototype(method).New());
        service->CallMethod(method, NULL, get_pointer(request), get_pointer(response), nullptr);
        RpcMessage rpc_response;
        rpc_response.set_type(RESPONSE);
        rpc_response.set_id(message.id());
        rpc_response.set_response(response->SerializeAsString()); // FIXME: error check
        rpc_response.set_message_id(message.message_id());
        codec_.send(conn_, rpc_response);
    }
}

void RpcChannel::SendRpcError(const RpcMessage& message, ErrorCode error)
{
    RpcMessage response;
    response.set_type(RESPONSE);
    response.set_id(message.id());
    response.set_error(error);
    codec_.send(conn_, response);
}

void RpcChannel::SendRouteResponse(uint32_t message_id, uint64_t id, const std::string&& message_bytes)
{
    //todo check message id error
    RpcMessage rpc_response;
    rpc_response.set_type(RESPONSE);
    rpc_response.set_id(id);
    rpc_response.set_response(message_bytes); // FIXME: error check
    rpc_response.set_message_id(message_id);
    codec_.send(conn_, rpc_response);
}

