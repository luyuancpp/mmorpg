#include "grpc_etcd_transport.h"

#include <muduo/base/Logging.h>

#include "grpc_client/etcd/etcd_grpc_client.h"
#include "grpc_client/grpc_init_client.h"
#include "node/system/grpc_channel_cache.h"
#include "node/system/node/node.h"
#include "thread_context/node_context_manager.h"

namespace
{
entt::registry &EtcdRegistry()
{
	return tlsNodeContextManager.GetRegistry(EtcdNodeService);
}

entt::entity EtcdEntity()
{
	return tlsNodeContextManager.GetGlobalEntity(EtcdNodeService);
}
} // namespace

GrpcEtcdTransport::GrpcEtcdTransport(const std::string &endpoint, EtcdResponseSink &sink)
	: sink_(sink)
{
	InstallHandlers();

	auto &grpcChannelCache = gNode->GetGrpcChannelCache();
	auto channel = grpcChannelCache.GetOrCreateChannel(endpoint);
	LOG_INFO << "gRPC client config: ResourceQuota max threads=" << grpcChannelCache.ConfiguredMaxThreads()
			 << ", backup poll interval ms=" << grpcChannelCache.ConfiguredBackupPollIntervalMs();

	// GetOrCreateGlobalEntity(不是 GetGlobalEntity):etcd 客户端的全部状态
	// (CompletionQueue / KV / Watch / Lease stub / watch 流)都挂在这个全局
	// 实体上,它必须真正经 registry.create() 诞生。此前配合 globalEntities_
	// 未初始化的 bug,这里拿到的是零残留的幽灵 entity 0,从未 create 过,
	// 全靠 EnTT 池语义宽容才能跑;修掉初始化后若仍用 GetGlobalEntity,
	// 拿到的会是 entt::null,emplace 直接 UB。之后的发送路径都在构造之后执行,
	// 继续用 GetGlobalEntity 拿的就是这里创建好的实体。
	InitGrpcNode(channel, EtcdRegistry(), tlsNodeContextManager.GetOrCreateGlobalEntity(EtcdNodeService));
}

GrpcEtcdTransport::~GrpcEtcdTransport()
{
	Shutdown();
}

void GrpcEtcdTransport::InstallHandlers()
{
	// 与改前 EtcdService::InitHandlers 装的是同一组 handler,只是转给 sink;没装的(Revoke / TimeToLive / Leases)
	// 照旧留给各节点 main 的 SetIfEmptyHandler。
	etcdserverpb::AsyncKVRangeHandler = [this](const ClientContext &, const etcdserverpb::RangeResponse &reply)
	{ sink_.OnRange(nullptr, reply); };
	etcdserverpb::AsyncKVPutHandler = [this](const ClientContext &, const etcdserverpb::PutResponse &reply)
	{ sink_.OnPut(reply); };
	etcdserverpb::AsyncKVDeleteRangeHandler = [this](const ClientContext &, const etcdserverpb::DeleteRangeResponse &reply)
	{ sink_.OnDeleteRange(reply); };
	if (!etcdserverpb::AsyncKVCompactHandler)
	{
		etcdserverpb::AsyncKVCompactHandler = [](const ClientContext &, const ::google::protobuf::Message &) {};
	}
	etcdserverpb::AsyncKVTxnHandler = [this](const ClientContext &, const etcdserverpb::TxnResponse &reply)
	{ sink_.OnTxn(reply); };
	etcdserverpb::AsyncWatchWatchHandler = [this](const ClientContext &, const etcdserverpb::WatchResponse &reply)
	{ sink_.OnWatch(reply); };
	etcdserverpb::AsyncLeaseLeaseGrantHandler = [this](const ClientContext &, const etcdserverpb::LeaseGrantResponse &reply)
	{ sink_.OnLeaseGrant(reply); };
	etcdserverpb::AsyncLeaseLeaseKeepAliveHandler =
		[this](const ClientContext &, const etcdserverpb::LeaseKeepAliveResponse &reply) { sink_.OnKeepAlive(reply); };
}

void GrpcEtcdTransport::Range(const etcdserverpb::RangeRequest &request)
{
	etcdserverpb::SendKVRange(EtcdRegistry(), EtcdEntity(), request);
}

void GrpcEtcdTransport::Put(const etcdserverpb::PutRequest &request)
{
	etcdserverpb::SendKVPut(EtcdRegistry(), EtcdEntity(), request);
}

void GrpcEtcdTransport::DeleteRange(const etcdserverpb::DeleteRangeRequest &request)
{
	etcdserverpb::SendKVDeleteRange(EtcdRegistry(), EtcdEntity(), request);
}

void GrpcEtcdTransport::Txn(const etcdserverpb::TxnRequest &request)
{
	etcdserverpb::SendKVTxn(EtcdRegistry(), EtcdEntity(), request);
}

void GrpcEtcdTransport::LeaseGrant(const etcdserverpb::LeaseGrantRequest &request)
{
	etcdserverpb::SendLeaseLeaseGrant(EtcdRegistry(), EtcdEntity(), request);
}

void GrpcEtcdTransport::LeaseRevoke(const etcdserverpb::LeaseRevokeRequest &request)
{
	etcdserverpb::SendLeaseLeaseRevoke(EtcdRegistry(), EtcdEntity(), request);
}

void GrpcEtcdTransport::WatchWrite(const etcdserverpb::WatchRequest &request)
{
	etcdserverpb::SendWatchWatch(EtcdRegistry(), EtcdEntity(), request);
}

void GrpcEtcdTransport::KeepAliveWrite(const etcdserverpb::LeaseKeepAliveRequest &request)
{
	etcdserverpb::SendLeaseLeaseKeepAlive(EtcdRegistry(), EtcdEntity(), request);
}

void GrpcEtcdTransport::Shutdown()
{
	// 一次清空全部生成层的 etcd 应答 handler(含各节点 main 经 SetIfEmptyHandler 塞进来的闭包),
	// 而不是手列:见 docs/ops/incident-gate-tcpconnection-dtor-assert-2026-09-13.md §7.13。
	// CQ 与 stub 仍挂在 EtcdNodeService 的全局实体上,由 Node 关机的 CQ 排空统一回收,不在这里动。
	etcdserverpb::SetEtcdHandler([](const ClientContext &, const ::google::protobuf::Message &) {});
}
