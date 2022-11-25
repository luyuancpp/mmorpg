// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "rpc_channel.h"

#include "muduo/base/Logging.h"

#include <google/protobuf/descriptor.h>
#include <google/protobuf/empty.pb.h>

#include "game_rpc.pb.h"

using namespace muduo;
using namespace muduo::net;

extern std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> g_prototype_services;

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
  LOG_INFO << "RpcChannel::ctor - " << this;
}

RpcChannel::RpcChannel(const TcpConnectionPtr& conn)
  : codec_(std::bind(&RpcChannel::onRpcMessage, this, _1, _2, _3)),
    conn_(conn),
    services_(NULL),
    dispatcher_(std::bind(&RpcChannel::onUnknownMessage, this, _1, _2, _3))
{
  LOG_INFO << "RpcChannel::ctor - " << this;
}

RpcChannel::~RpcChannel()
{
  LOG_INFO << "RpcChannel::dtor - " << this;
  for (const auto& outstanding : outstandings_)
  {
    OutstandingCall out = outstanding.second;
    //delete out.response;
    delete out.done;
  }
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
  int64_t id = id_.incrementAndGet();
  message.set_id(id);
  message.set_service(method->service()->full_name());
  message.set_method(method->name());
  message.set_request(request->SerializeAsString()); // FIXME: error check

  auto service_it = g_prototype_services.find(method->service()->full_name());
  if (service_it == g_prototype_services.end())
  {
      return;
  }
  OutstandingCall out = { method, service_it->second.get() , done };
  {
#ifdef MUDUO_RPC_LOCK
	 MutexLockGuard lock(mutex_);
#endif // MUDUO_RPC_LOCK
  outstandings_[id] = out;
  }
  codec_.send(conn_, message);
}

void RpcChannel::Send(const ::google::protobuf::MethodDescriptor* method, const ::google::protobuf::Message& request)
{
    RpcMessage message;    
    message.set_type(S2C_REQUEST);
    message.set_request(request.SerializeAsString()); // FIXME: error check
    message.set_service(method->service()->full_name());
    message.set_method(method->name());
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
  //printf("%s\n", message.DebugString().c_str());
  RpcMessage& message = *messagePtr;
  if (message.type() == RESPONSE)
  {
    int64_t id = message.id();
 
    OutstandingCall out = { NULL, NULL };

    {
#ifdef MUDUO_RPC_LOCK
      MutexLockGuard lock(mutex_);
#endif // MUDUO_RPC_LOCK
      std::map<int64_t, OutstandingCall>::iterator it = outstandings_.find(id);
      if (it != outstandings_.end())
      {
        out = it->second;
        outstandings_.erase(it);
      }
    }

    if (out.method && out.service)
    {
      MessagePtr response(out.service->GetRequestPrototype(out.method).New());
      if (!message.response().empty())
      {
          response->ParseFromString(message.response());
      }
      g_response_dispatcher.onProtobufMessage(conn, response, receiveTime);
      /* if (out.done)
       {
         out.done->Run();
       }*/
    }
  }
  else if (message.type() == REQUEST)
  {
    // FIXME: extract to a function
    ErrorCode error = WRONG_PROTO;
    if (services_)
    {
      std::map<std::string, google::protobuf::Service*>::const_iterator it = services_->find(message.service());
      if (it != services_->end())
      {
        google::protobuf::Service* service = it->second;
        assert(service != NULL);
        const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
        const google::protobuf::MethodDescriptor* method
          = desc->FindMethodByName(message.method());
        if (method)
        {
          std::unique_ptr<google::protobuf::Message> request(service->GetRequestPrototype(method).New());
          if (request->ParseFromString(message.request()))
          {
			  auto& prototype = service->GetResponsePrototype(method);
			  if (prototype.GetDescriptor() == ::google::protobuf::Empty::GetDescriptor())
			  {
				  service->CallMethod(method, NULL, get_pointer(request), nullptr, nullptr);
			  }
              else
              {
				  google::protobuf::Message* response = service->GetResponsePrototype(method).New();
				  // response is deleted in doneCallback
				  int64_t id = message.id();
				  service->CallMethod(method, NULL, get_pointer(request), response,
					  NewCallback(this, &RpcChannel::doneCallback, response, id));
              }
            error = RPC_NO_ERROR;
          }
          else
          {
            error = INVALID_REQUEST;
          }
        }
        else
        {
          error = NO_METHOD;
        }
      }
      else
      {
        error = NO_SERVICE;
      }
    }
    else
    {
      error = NO_SERVICE;
    }
    if (error != RPC_NO_ERROR)
    {
      RpcMessage response;
      response.set_type(RESPONSE);
      response.set_id(message.id());
      response.set_error(error);
      codec_.send(conn_, response);
    }
  }
  else if (message.type() == S2C_REQUEST)
  {
      // FIXME: extract to a function
      if (services_)
      {
          std::map<std::string, google::protobuf::Service*>::const_iterator it = services_->find(message.service());
          if (it != services_->end())
          {
              google::protobuf::Service* service = it->second;
              assert(service != NULL);
              const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
              const google::protobuf::MethodDescriptor* method
                  = desc->FindMethodByName(message.method());
              if (method)
              {
                  std::unique_ptr<google::protobuf::Message> request(service->GetRequestPrototype(method).New());
                  if (request->ParseFromString(message.request()))
                  {
                      service->CallMethod(method, nullptr, get_pointer(request), nullptr,
                          NewCallback(this, &RpcChannel::doNothing));
                  }
              }
         }
      }
  }
  else if (message.type() == RPC_ERROR)
  {
  }
}

void RpcChannel::doneCallback(::google::protobuf::Message* response, int64_t id)
{
  std::unique_ptr<google::protobuf::Message> d(response);
  RpcMessage message;
  message.set_type(RESPONSE);
  message.set_id(id);
  message.set_response(response->SerializeAsString()); // FIXME: error check
  codec_.send(conn_, message);
}


