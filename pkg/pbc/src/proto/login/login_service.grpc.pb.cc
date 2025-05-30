// Generated by the gRPC C++ plugin.
// If you make any local change, they will be lost.
// source: proto/login/login_service.proto

#include "proto/login/login_service.pb.h"
#include "proto/login/login_service.grpc.pb.h"

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
namespace loginpb {

static const char* LoginService_method_names[] = {
  "/loginpb.LoginService/Login",
  "/loginpb.LoginService/CreatePlayer",
  "/loginpb.LoginService/EnterGame",
  "/loginpb.LoginService/LeaveGame",
  "/loginpb.LoginService/Disconnect",
};

std::unique_ptr< LoginService::Stub> LoginService::NewStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options) {
  (void)options;
  std::unique_ptr< LoginService::Stub> stub(new LoginService::Stub(channel, options));
  return stub;
}

LoginService::Stub::Stub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options)
  : channel_(channel), rpcmethod_Login_(LoginService_method_names[0], options.suffix_for_stats(),::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_CreatePlayer_(LoginService_method_names[1], options.suffix_for_stats(),::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_EnterGame_(LoginService_method_names[2], options.suffix_for_stats(),::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_LeaveGame_(LoginService_method_names[3], options.suffix_for_stats(),::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_Disconnect_(LoginService_method_names[4], options.suffix_for_stats(),::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  {}

::grpc::Status LoginService::Stub::Login(::grpc::ClientContext* context, const ::loginpb::LoginC2LRequest& request, ::loginpb::LoginC2LResponse* response) {
  return ::grpc::internal::BlockingUnaryCall< ::loginpb::LoginC2LRequest, ::loginpb::LoginC2LResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_Login_, context, request, response);
}

void LoginService::Stub::async::Login(::grpc::ClientContext* context, const ::loginpb::LoginC2LRequest* request, ::loginpb::LoginC2LResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::loginpb::LoginC2LRequest, ::loginpb::LoginC2LResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_Login_, context, request, response, std::move(f));
}

void LoginService::Stub::async::Login(::grpc::ClientContext* context, const ::loginpb::LoginC2LRequest* request, ::loginpb::LoginC2LResponse* response, ::grpc::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_Login_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::loginpb::LoginC2LResponse>* LoginService::Stub::PrepareAsyncLoginRaw(::grpc::ClientContext* context, const ::loginpb::LoginC2LRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::loginpb::LoginC2LResponse, ::loginpb::LoginC2LRequest, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_Login_, context, request);
}

::grpc::ClientAsyncResponseReader< ::loginpb::LoginC2LResponse>* LoginService::Stub::AsyncLoginRaw(::grpc::ClientContext* context, const ::loginpb::LoginC2LRequest& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncLoginRaw(context, request, cq);
  result->StartCall();
  return result;
}

::grpc::Status LoginService::Stub::CreatePlayer(::grpc::ClientContext* context, const ::loginpb::CreatePlayerC2LRequest& request, ::loginpb::CreatePlayerC2LResponse* response) {
  return ::grpc::internal::BlockingUnaryCall< ::loginpb::CreatePlayerC2LRequest, ::loginpb::CreatePlayerC2LResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_CreatePlayer_, context, request, response);
}

void LoginService::Stub::async::CreatePlayer(::grpc::ClientContext* context, const ::loginpb::CreatePlayerC2LRequest* request, ::loginpb::CreatePlayerC2LResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::loginpb::CreatePlayerC2LRequest, ::loginpb::CreatePlayerC2LResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_CreatePlayer_, context, request, response, std::move(f));
}

void LoginService::Stub::async::CreatePlayer(::grpc::ClientContext* context, const ::loginpb::CreatePlayerC2LRequest* request, ::loginpb::CreatePlayerC2LResponse* response, ::grpc::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_CreatePlayer_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::loginpb::CreatePlayerC2LResponse>* LoginService::Stub::PrepareAsyncCreatePlayerRaw(::grpc::ClientContext* context, const ::loginpb::CreatePlayerC2LRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::loginpb::CreatePlayerC2LResponse, ::loginpb::CreatePlayerC2LRequest, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_CreatePlayer_, context, request);
}

::grpc::ClientAsyncResponseReader< ::loginpb::CreatePlayerC2LResponse>* LoginService::Stub::AsyncCreatePlayerRaw(::grpc::ClientContext* context, const ::loginpb::CreatePlayerC2LRequest& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncCreatePlayerRaw(context, request, cq);
  result->StartCall();
  return result;
}

::grpc::Status LoginService::Stub::EnterGame(::grpc::ClientContext* context, const ::loginpb::EnterGameC2LRequest& request, ::loginpb::EnterGameC2LResponse* response) {
  return ::grpc::internal::BlockingUnaryCall< ::loginpb::EnterGameC2LRequest, ::loginpb::EnterGameC2LResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_EnterGame_, context, request, response);
}

void LoginService::Stub::async::EnterGame(::grpc::ClientContext* context, const ::loginpb::EnterGameC2LRequest* request, ::loginpb::EnterGameC2LResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::loginpb::EnterGameC2LRequest, ::loginpb::EnterGameC2LResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_EnterGame_, context, request, response, std::move(f));
}

void LoginService::Stub::async::EnterGame(::grpc::ClientContext* context, const ::loginpb::EnterGameC2LRequest* request, ::loginpb::EnterGameC2LResponse* response, ::grpc::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_EnterGame_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::loginpb::EnterGameC2LResponse>* LoginService::Stub::PrepareAsyncEnterGameRaw(::grpc::ClientContext* context, const ::loginpb::EnterGameC2LRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::loginpb::EnterGameC2LResponse, ::loginpb::EnterGameC2LRequest, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_EnterGame_, context, request);
}

::grpc::ClientAsyncResponseReader< ::loginpb::EnterGameC2LResponse>* LoginService::Stub::AsyncEnterGameRaw(::grpc::ClientContext* context, const ::loginpb::EnterGameC2LRequest& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncEnterGameRaw(context, request, cq);
  result->StartCall();
  return result;
}

::grpc::Status LoginService::Stub::LeaveGame(::grpc::ClientContext* context, const ::loginpb::LeaveGameC2LRequest& request, ::Empty* response) {
  return ::grpc::internal::BlockingUnaryCall< ::loginpb::LeaveGameC2LRequest, ::Empty, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_LeaveGame_, context, request, response);
}

void LoginService::Stub::async::LeaveGame(::grpc::ClientContext* context, const ::loginpb::LeaveGameC2LRequest* request, ::Empty* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::loginpb::LeaveGameC2LRequest, ::Empty, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_LeaveGame_, context, request, response, std::move(f));
}

void LoginService::Stub::async::LeaveGame(::grpc::ClientContext* context, const ::loginpb::LeaveGameC2LRequest* request, ::Empty* response, ::grpc::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_LeaveGame_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::Empty>* LoginService::Stub::PrepareAsyncLeaveGameRaw(::grpc::ClientContext* context, const ::loginpb::LeaveGameC2LRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::Empty, ::loginpb::LeaveGameC2LRequest, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_LeaveGame_, context, request);
}

::grpc::ClientAsyncResponseReader< ::Empty>* LoginService::Stub::AsyncLeaveGameRaw(::grpc::ClientContext* context, const ::loginpb::LeaveGameC2LRequest& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncLeaveGameRaw(context, request, cq);
  result->StartCall();
  return result;
}

::grpc::Status LoginService::Stub::Disconnect(::grpc::ClientContext* context, const ::loginpb::LoginNodeDisconnectRequest& request, ::Empty* response) {
  return ::grpc::internal::BlockingUnaryCall< ::loginpb::LoginNodeDisconnectRequest, ::Empty, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_Disconnect_, context, request, response);
}

void LoginService::Stub::async::Disconnect(::grpc::ClientContext* context, const ::loginpb::LoginNodeDisconnectRequest* request, ::Empty* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::loginpb::LoginNodeDisconnectRequest, ::Empty, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_Disconnect_, context, request, response, std::move(f));
}

void LoginService::Stub::async::Disconnect(::grpc::ClientContext* context, const ::loginpb::LoginNodeDisconnectRequest* request, ::Empty* response, ::grpc::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_Disconnect_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::Empty>* LoginService::Stub::PrepareAsyncDisconnectRaw(::grpc::ClientContext* context, const ::loginpb::LoginNodeDisconnectRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::Empty, ::loginpb::LoginNodeDisconnectRequest, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_Disconnect_, context, request);
}

::grpc::ClientAsyncResponseReader< ::Empty>* LoginService::Stub::AsyncDisconnectRaw(::grpc::ClientContext* context, const ::loginpb::LoginNodeDisconnectRequest& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncDisconnectRaw(context, request, cq);
  result->StartCall();
  return result;
}

LoginService::Service::Service() {
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      LoginService_method_names[0],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< LoginService::Service, ::loginpb::LoginC2LRequest, ::loginpb::LoginC2LResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](LoginService::Service* service,
             ::grpc::ServerContext* ctx,
             const ::loginpb::LoginC2LRequest* req,
             ::loginpb::LoginC2LResponse* resp) {
               return service->Login(ctx, req, resp);
             }, this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      LoginService_method_names[1],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< LoginService::Service, ::loginpb::CreatePlayerC2LRequest, ::loginpb::CreatePlayerC2LResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](LoginService::Service* service,
             ::grpc::ServerContext* ctx,
             const ::loginpb::CreatePlayerC2LRequest* req,
             ::loginpb::CreatePlayerC2LResponse* resp) {
               return service->CreatePlayer(ctx, req, resp);
             }, this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      LoginService_method_names[2],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< LoginService::Service, ::loginpb::EnterGameC2LRequest, ::loginpb::EnterGameC2LResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](LoginService::Service* service,
             ::grpc::ServerContext* ctx,
             const ::loginpb::EnterGameC2LRequest* req,
             ::loginpb::EnterGameC2LResponse* resp) {
               return service->EnterGame(ctx, req, resp);
             }, this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      LoginService_method_names[3],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< LoginService::Service, ::loginpb::LeaveGameC2LRequest, ::Empty, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](LoginService::Service* service,
             ::grpc::ServerContext* ctx,
             const ::loginpb::LeaveGameC2LRequest* req,
             ::Empty* resp) {
               return service->LeaveGame(ctx, req, resp);
             }, this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      LoginService_method_names[4],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< LoginService::Service, ::loginpb::LoginNodeDisconnectRequest, ::Empty, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](LoginService::Service* service,
             ::grpc::ServerContext* ctx,
             const ::loginpb::LoginNodeDisconnectRequest* req,
             ::Empty* resp) {
               return service->Disconnect(ctx, req, resp);
             }, this)));
}

LoginService::Service::~Service() {
}

::grpc::Status LoginService::Service::Login(::grpc::ServerContext* context, const ::loginpb::LoginC2LRequest* request, ::loginpb::LoginC2LResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status LoginService::Service::CreatePlayer(::grpc::ServerContext* context, const ::loginpb::CreatePlayerC2LRequest* request, ::loginpb::CreatePlayerC2LResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status LoginService::Service::EnterGame(::grpc::ServerContext* context, const ::loginpb::EnterGameC2LRequest* request, ::loginpb::EnterGameC2LResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status LoginService::Service::LeaveGame(::grpc::ServerContext* context, const ::loginpb::LeaveGameC2LRequest* request, ::Empty* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status LoginService::Service::Disconnect(::grpc::ServerContext* context, const ::loginpb::LoginNodeDisconnectRequest* request, ::Empty* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}


}  // namespace loginpb

