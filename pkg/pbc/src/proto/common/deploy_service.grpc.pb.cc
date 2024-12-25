// Generated by the gRPC C++ plugin.
// If you make any local change, they will be lost.
// source: proto/common/deploy_service.proto

#include "proto/common/deploy_service.pb.h"
#include "proto/common/deploy_service.grpc.pb.h"

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

static const char* DeployService_method_names[] = {
  "/DeployService/GetNodeInfo",
};

std::unique_ptr< DeployService::Stub> DeployService::NewStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options) {
  (void)options;
  std::unique_ptr< DeployService::Stub> stub(new DeployService::Stub(channel, options));
  return stub;
}

DeployService::Stub::Stub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options)
  : channel_(channel), rpcmethod_GetNodeInfo_(DeployService_method_names[0], options.suffix_for_stats(),::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  {}

::grpc::Status DeployService::Stub::GetNodeInfo(::grpc::ClientContext* context, const ::NodeInfoRequest& request, ::NodeInfoResponse* response) {
  return ::grpc::internal::BlockingUnaryCall< ::NodeInfoRequest, ::NodeInfoResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_GetNodeInfo_, context, request, response);
}

void DeployService::Stub::async::GetNodeInfo(::grpc::ClientContext* context, const ::NodeInfoRequest* request, ::NodeInfoResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::NodeInfoRequest, ::NodeInfoResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_GetNodeInfo_, context, request, response, std::move(f));
}

void DeployService::Stub::async::GetNodeInfo(::grpc::ClientContext* context, const ::NodeInfoRequest* request, ::NodeInfoResponse* response, ::grpc::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_GetNodeInfo_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::NodeInfoResponse>* DeployService::Stub::PrepareAsyncGetNodeInfoRaw(::grpc::ClientContext* context, const ::NodeInfoRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::NodeInfoResponse, ::NodeInfoRequest, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_GetNodeInfo_, context, request);
}

::grpc::ClientAsyncResponseReader< ::NodeInfoResponse>* DeployService::Stub::AsyncGetNodeInfoRaw(::grpc::ClientContext* context, const ::NodeInfoRequest& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncGetNodeInfoRaw(context, request, cq);
  result->StartCall();
  return result;
}

DeployService::Service::Service() {
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      DeployService_method_names[0],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< DeployService::Service, ::NodeInfoRequest, ::NodeInfoResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](DeployService::Service* service,
             ::grpc::ServerContext* ctx,
             const ::NodeInfoRequest* req,
             ::NodeInfoResponse* resp) {
               return service->GetNodeInfo(ctx, req, resp);
             }, this)));
}

DeployService::Service::~Service() {
}

::grpc::Status DeployService::Service::GetNodeInfo(::grpc::ServerContext* context, const ::NodeInfoRequest* request, ::NodeInfoResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}


