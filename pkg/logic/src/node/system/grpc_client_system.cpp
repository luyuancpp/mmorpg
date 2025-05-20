#include "grpc_client_system.h"
#include <grpcpp/create_channel.h>
#include "grpc/generator/proto/etcd/etcd_grpc.h"
#include "grpc/generator/proto/common/deploy_service_grpc.h"
#include "thread_local/storage_common_logic.h"
#include <thread_local/storage.h>

void GrpcClientSystem::InitEtcdStubs(const ::google::protobuf::RepeatedPtrField<std::string>& etcdHosts) {
	const std::string& etcdAddr = *etcdHosts.begin();
	auto channel = grpc::CreateChannel(etcdAddr, grpc::InsecureChannelCredentials());

	tls.globalNodeRegistry.emplace<GrpcetcdserverpbKVStubPtr>(GetGlobalGrpcNodeEntity()) =
		etcdserverpb::KV::NewStub(channel);

	tls.globalNodeRegistry.emplace<GrpcetcdserverpbWatchStubPtr>(GetGlobalGrpcNodeEntity()) =
		etcdserverpb::Watch::NewStub(channel);

	tls.globalNodeRegistry.emplace<GrpcetcdserverpbLeaseStubPtr>(GetGlobalGrpcNodeEntity()) =
		etcdserverpb::Lease::NewStub(channel);
}

void GrpcClientSystem::InitDeployService(const std::string& deployAddress) {
	auto channel = grpc::CreateChannel(deployAddress, grpc::InsecureChannelCredentials());
	tls.globalNodeRegistry.emplace<GrpcDeployServiceStubPtr>(GetGlobalGrpcNodeEntity()) =
		DeployService::NewStub(channel);
}
