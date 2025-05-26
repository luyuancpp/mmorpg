#include "grpc_client_system.h"
#include <grpcpp/create_channel.h>
#include "grpc/generator/proto/etcd/etcd_grpc.h"
#include <thread_local/storage.h>

void GrpcClientSystem::InitEtcdStubs(const ::google::protobuf::RepeatedPtrField<std::string>& etcdHosts) {
	const std::string& etcdAddr = *etcdHosts.begin();
	auto channel = grpc::CreateChannel(etcdAddr, grpc::InsecureChannelCredentials());

	tls.globalNodeRegistry.emplace<etcdserverpb::KVStubPtr>(GetGlobalGrpcNodeEntity()) =
		etcdserverpb::KV::NewStub(channel);

	tls.globalNodeRegistry.emplace<etcdserverpb::WatchStubPtr>(GetGlobalGrpcNodeEntity()) =
		etcdserverpb::Watch::NewStub(channel);

	tls.globalNodeRegistry.emplace<etcdserverpb::LeaseStubPtr>(GetGlobalGrpcNodeEntity()) =
		etcdserverpb::Lease::NewStub(channel);
}

