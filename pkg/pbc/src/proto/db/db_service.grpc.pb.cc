// Generated by the gRPC C++ plugin.
// If you make any local change, they will be lost.
// source: proto/db/db_service.proto

#include "proto/db/db_service.pb.h"
#include "proto/db/db_service.grpc.pb.h"

#include <functional>
#include <grpcpp/support/async_stream.h>
#include <grpcpp/support/async_unary_call.h>
#include <grpcpp/impl/channel_interface.h>
#include <grpcpp/impl/client_unary_call.h>
#include <grpcpp/support/client_callback.h>
#include <grpcpp/support/message_allocator.h>
#include <grpcpp/support/method_handler.h>
#include <grpcpp/impl/rpc_service_method.h>
#include <grpcpp/support/server_callback.h>
#include <grpcpp/impl/server_callback_handlers.h>
#include <grpcpp/server_context.h>
#include <grpcpp/impl/service_type.h>
#include <grpcpp/support/sync_stream.h>

static const char* AccountDBService_method_names[] = {
  "/AccountDBService/Load2Redis",
  "/AccountDBService/Save2Redis",
};

std::unique_ptr< AccountDBService::Stub> AccountDBService::NewStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options) {
  (void)options;
  std::unique_ptr< AccountDBService::Stub> stub(new AccountDBService::Stub(channel, options));
  return stub;
}

AccountDBService::Stub::Stub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options)
  : channel_(channel), rpcmethod_Load2Redis_(AccountDBService_method_names[0], options.suffix_for_stats(),::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_Save2Redis_(AccountDBService_method_names[1], options.suffix_for_stats(),::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  {}

::grpc::Status AccountDBService::Stub::Load2Redis(::grpc::ClientContext* context, const ::LoadAccountRequest& request, ::LoadAccountResponse* response) {
  return ::grpc::internal::BlockingUnaryCall< ::LoadAccountRequest, ::LoadAccountResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_Load2Redis_, context, request, response);
}

void AccountDBService::Stub::async::Load2Redis(::grpc::ClientContext* context, const ::LoadAccountRequest* request, ::LoadAccountResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::LoadAccountRequest, ::LoadAccountResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_Load2Redis_, context, request, response, std::move(f));
}

void AccountDBService::Stub::async::Load2Redis(::grpc::ClientContext* context, const ::LoadAccountRequest* request, ::LoadAccountResponse* response, ::grpc::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_Load2Redis_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::LoadAccountResponse>* AccountDBService::Stub::PrepareAsyncLoad2RedisRaw(::grpc::ClientContext* context, const ::LoadAccountRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::LoadAccountResponse, ::LoadAccountRequest, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_Load2Redis_, context, request);
}

::grpc::ClientAsyncResponseReader< ::LoadAccountResponse>* AccountDBService::Stub::AsyncLoad2RedisRaw(::grpc::ClientContext* context, const ::LoadAccountRequest& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncLoad2RedisRaw(context, request, cq);
  result->StartCall();
  return result;
}

::grpc::Status AccountDBService::Stub::Save2Redis(::grpc::ClientContext* context, const ::SaveAccountRequest& request, ::SaveAccountResponse* response) {
  return ::grpc::internal::BlockingUnaryCall< ::SaveAccountRequest, ::SaveAccountResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_Save2Redis_, context, request, response);
}

void AccountDBService::Stub::async::Save2Redis(::grpc::ClientContext* context, const ::SaveAccountRequest* request, ::SaveAccountResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::SaveAccountRequest, ::SaveAccountResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_Save2Redis_, context, request, response, std::move(f));
}

void AccountDBService::Stub::async::Save2Redis(::grpc::ClientContext* context, const ::SaveAccountRequest* request, ::SaveAccountResponse* response, ::grpc::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_Save2Redis_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::SaveAccountResponse>* AccountDBService::Stub::PrepareAsyncSave2RedisRaw(::grpc::ClientContext* context, const ::SaveAccountRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::SaveAccountResponse, ::SaveAccountRequest, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_Save2Redis_, context, request);
}

::grpc::ClientAsyncResponseReader< ::SaveAccountResponse>* AccountDBService::Stub::AsyncSave2RedisRaw(::grpc::ClientContext* context, const ::SaveAccountRequest& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncSave2RedisRaw(context, request, cq);
  result->StartCall();
  return result;
}

AccountDBService::Service::Service() {
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      AccountDBService_method_names[0],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< AccountDBService::Service, ::LoadAccountRequest, ::LoadAccountResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](AccountDBService::Service* service,
             ::grpc::ServerContext* ctx,
             const ::LoadAccountRequest* req,
             ::LoadAccountResponse* resp) {
               return service->Load2Redis(ctx, req, resp);
             }, this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      AccountDBService_method_names[1],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< AccountDBService::Service, ::SaveAccountRequest, ::SaveAccountResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](AccountDBService::Service* service,
             ::grpc::ServerContext* ctx,
             const ::SaveAccountRequest* req,
             ::SaveAccountResponse* resp) {
               return service->Save2Redis(ctx, req, resp);
             }, this)));
}

AccountDBService::Service::~Service() {
}

::grpc::Status AccountDBService::Service::Load2Redis(::grpc::ServerContext* context, const ::LoadAccountRequest* request, ::LoadAccountResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status AccountDBService::Service::Save2Redis(::grpc::ServerContext* context, const ::SaveAccountRequest* request, ::SaveAccountResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}


static const char* PlayerDBService_method_names[] = {
  "/PlayerDBService/Load2Redis",
  "/PlayerDBService/Save2Redis",
};

std::unique_ptr< PlayerDBService::Stub> PlayerDBService::NewStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options) {
  (void)options;
  std::unique_ptr< PlayerDBService::Stub> stub(new PlayerDBService::Stub(channel, options));
  return stub;
}

PlayerDBService::Stub::Stub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options)
  : channel_(channel), rpcmethod_Load2Redis_(PlayerDBService_method_names[0], options.suffix_for_stats(),::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_Save2Redis_(PlayerDBService_method_names[1], options.suffix_for_stats(),::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  {}

::grpc::Status PlayerDBService::Stub::Load2Redis(::grpc::ClientContext* context, const ::LoadPlayerRequest& request, ::LoadPlayerResponse* response) {
  return ::grpc::internal::BlockingUnaryCall< ::LoadPlayerRequest, ::LoadPlayerResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_Load2Redis_, context, request, response);
}

void PlayerDBService::Stub::async::Load2Redis(::grpc::ClientContext* context, const ::LoadPlayerRequest* request, ::LoadPlayerResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::LoadPlayerRequest, ::LoadPlayerResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_Load2Redis_, context, request, response, std::move(f));
}

void PlayerDBService::Stub::async::Load2Redis(::grpc::ClientContext* context, const ::LoadPlayerRequest* request, ::LoadPlayerResponse* response, ::grpc::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_Load2Redis_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::LoadPlayerResponse>* PlayerDBService::Stub::PrepareAsyncLoad2RedisRaw(::grpc::ClientContext* context, const ::LoadPlayerRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::LoadPlayerResponse, ::LoadPlayerRequest, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_Load2Redis_, context, request);
}

::grpc::ClientAsyncResponseReader< ::LoadPlayerResponse>* PlayerDBService::Stub::AsyncLoad2RedisRaw(::grpc::ClientContext* context, const ::LoadPlayerRequest& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncLoad2RedisRaw(context, request, cq);
  result->StartCall();
  return result;
}

::grpc::Status PlayerDBService::Stub::Save2Redis(::grpc::ClientContext* context, const ::SavePlayerRequest& request, ::SavePlayerResponse* response) {
  return ::grpc::internal::BlockingUnaryCall< ::SavePlayerRequest, ::SavePlayerResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_Save2Redis_, context, request, response);
}

void PlayerDBService::Stub::async::Save2Redis(::grpc::ClientContext* context, const ::SavePlayerRequest* request, ::SavePlayerResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::SavePlayerRequest, ::SavePlayerResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_Save2Redis_, context, request, response, std::move(f));
}

void PlayerDBService::Stub::async::Save2Redis(::grpc::ClientContext* context, const ::SavePlayerRequest* request, ::SavePlayerResponse* response, ::grpc::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_Save2Redis_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::SavePlayerResponse>* PlayerDBService::Stub::PrepareAsyncSave2RedisRaw(::grpc::ClientContext* context, const ::SavePlayerRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::SavePlayerResponse, ::SavePlayerRequest, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_Save2Redis_, context, request);
}

::grpc::ClientAsyncResponseReader< ::SavePlayerResponse>* PlayerDBService::Stub::AsyncSave2RedisRaw(::grpc::ClientContext* context, const ::SavePlayerRequest& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncSave2RedisRaw(context, request, cq);
  result->StartCall();
  return result;
}

PlayerDBService::Service::Service() {
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      PlayerDBService_method_names[0],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< PlayerDBService::Service, ::LoadPlayerRequest, ::LoadPlayerResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](PlayerDBService::Service* service,
             ::grpc::ServerContext* ctx,
             const ::LoadPlayerRequest* req,
             ::LoadPlayerResponse* resp) {
               return service->Load2Redis(ctx, req, resp);
             }, this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      PlayerDBService_method_names[1],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< PlayerDBService::Service, ::SavePlayerRequest, ::SavePlayerResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](PlayerDBService::Service* service,
             ::grpc::ServerContext* ctx,
             const ::SavePlayerRequest* req,
             ::SavePlayerResponse* resp) {
               return service->Save2Redis(ctx, req, resp);
             }, this)));
}

PlayerDBService::Service::~Service() {
}

::grpc::Status PlayerDBService::Service::Load2Redis(::grpc::ServerContext* context, const ::LoadPlayerRequest* request, ::LoadPlayerResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status PlayerDBService::Service::Save2Redis(::grpc::ServerContext* context, const ::SavePlayerRequest* request, ::SavePlayerResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}


