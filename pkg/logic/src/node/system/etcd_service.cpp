#include "etcd_service.h"
#include "etcd_helper.h"
#include "etcd_manager.h"
#include "node.h"  // 如果需要访问 gNode
#include <google/protobuf/util/json_util.h>
#include <boost/algorithm/string.hpp>

#include "node_allocator.h"
#include "grpc/generator/grpc_init.h"
#include "grpc/generator/proto/etcd/etcd_grpc.h"
#include "thread_local/storage_common_logic.h"

void EtcdService::Init() {
    InitHandlers();
    const std::string& etcdAddr = *tlsCommonLogic.GetBaseDeployConfig().etcd_hosts().begin();
    auto channel = grpc::CreateChannel(etcdAddr, grpc::InsecureChannelCredentials());

    InitGrpcNode(channel, tls.GetNodeRegistry(EtcdNodeService), tls.GetNodeGlobalEntity(EtcdNodeService));

    grpcHandlerTimer.RunEvery(0.005, [] {
        for (auto& registry : tls.GetNodeRegistry()) {
            HandleCompletedQueueMessage(registry);
        }
    });

    LOG_INFO << "EtcdService initialized with etcd: " << etcdAddr;
}

void EtcdService::InitHandlers() {
    InitKVHandlers();
    InitWatchHandlers();
    InitLeaseHandlers();
    InitTxnHandlers();
}

void EtcdService::InitKVHandlers() {
    etcdserverpb::AsyncKVRangeHandler = [this](const ClientContext& ctx, const etcdserverpb::RangeResponse& reply) {
        int64_t nextRevision = reply.header().revision() + 1;
        std::unordered_map<std::string, bool> prefixSeen;

        for (const auto& prefix : tlsCommonLogic.GetBaseDeployConfig().service_discovery_prefixes()) {
            prefixSeen[prefix] = false;
        }

        for (const auto& kv : reply.kvs()) {
            HandlePutEvent(kv.key(), kv.value());
            for (auto& [prefix, seen] : prefixSeen) {
                if (kv.key().rfind(prefix, 0) == 0) {
                    seen = true;
                }
            }
        }

        for (const auto& [prefix, _] : prefixSeen) {
            revision[prefix] = nextRevision;
        }

        if (!hasSentRange) {
            StartWatchingPrefixes();
            hasSentRange = true;
        }
    };

    etcdserverpb::AsyncKVPutHandler = [this](const ClientContext& context, const ::etcdserverpb::PutResponse& reply) {
        LOG_INFO << "Put response: " << reply.DebugString();
    };

    etcdserverpb::AsyncKVDeleteRangeHandler = [](const ClientContext& context, const ::etcdserverpb::DeleteRangeResponse& reply) {};

    auto emptyHandler = [](const ClientContext& context, const ::google::protobuf::Message& reply) {};
    if (!etcdserverpb::AsyncKVCompactHandler) {
        etcdserverpb::AsyncKVCompactHandler = emptyHandler;
    }
}

void EtcdService::InitWatchHandlers() {
    etcdserverpb::AsyncWatchWatchHandler = [this](const ClientContext& ctx, const etcdserverpb::WatchResponse& response) {
        OnWatchResponse(response);
    };
}

void EtcdService::InitLeaseHandlers() {
    etcdserverpb::AsyncLeaseLeaseGrantHandler = [this](const ClientContext& context, const ::etcdserverpb::LeaseGrantResponse& reply) {
        if (leaseId <= 0) {
            LOG_INFO << "Acquiring new lease, ID: " << reply.id();
            leaseId = reply.id();
            NodeAllocator::AcquireNodePort();
            NodeAllocator::AcquireNode();
            gNode->GetEtcdManager().KeepNodeAlive();
        } else {
            LOG_INFO << "Lease already exists, updating lease_id: " << reply.id();
            leaseId = reply.id();
            gNode->GetEtcdManager().KeepNodeAlive();
            gNode->GetEtcdManager().RegisterNodeService();
            gNode->GetEtcdManager().RegisterNodePort();
        }

        LOG_INFO << "Lease granted: " << reply.DebugString();
    };
    
}

void EtcdService::InitTxnHandlers() {
    etcdserverpb::AsyncKVTxnHandler = [this](const ClientContext& context, const ::etcdserverpb::TxnResponse& reply) {
        LOG_INFO << "Txn response: " << reply.DebugString();

        auto& key = pendingKeys.front();

        if (reply.succeeded()) {
            if (boost::algorithm::starts_with(key, gNode->GetEtcdManager().MakeNodePortEtcdPrefix(gNode->GetNodeInfo()))) {
                gNode->StartRpcServer();
            } else if (boost::algorithm::starts_with(key, gNode->GetEtcdManager().MakeNodeEtcdPrefix(gNode->GetNodeInfo()))) {
                tls.OnNodeStart(gNode->GetNodeInfo().node_id());
            }
        } else {
            if (boost::algorithm::starts_with(key, gNode->GetEtcdManager().MakeNodeEtcdPrefix(gNode->GetNodeInfo()))) {
                acquireNodeTimer.RunAfter(1, [this]() { NodeAllocator::AcquireNode(); });
            } else {
                acquirePortTimer.RunAfter(1, [this]() { NodeAllocator::AcquireNodePort(); });
            }
        }

        pendingKeys.pop_front();
    };
}

void EtcdService::StartWatchingPrefixes() {
    for (const auto& prefix : tlsCommonLogic.GetBaseDeployConfig().service_discovery_prefixes()) {
        EtcdHelper::StartWatchingPrefix(prefix, revision[prefix]);
        LOG_INFO << "Watching prefix: " << prefix << " from revision " << revision[prefix];
    }
}

void EtcdService::HandlePutEvent(const std::string& key, const std::string& value) {
    gNode->HandleServiceNodeStart(key, value);
}

void EtcdService::HandleDeleteEvent(const std::string& key, const std::string& value) {
    gNode->HandleServiceNodeStop(key, value);
}

void EtcdService::OnWatchResponse(const etcdserverpb::WatchResponse& response) {
    if (!hasSentWatch) {
        RequestLease();
        hasSentWatch = true;
    }

    if (response.canceled()) {
        LOG_INFO << "Watch canceled: " << response.cancel_reason();
        return;
    }

    for (const auto& event : response.events()) {
        if (event.type() == mvccpb::Event_EventType::Event_EventType_PUT) {
            HandlePutEvent(event.kv().key(), event.kv().value());
        } else if (event.type() == mvccpb::Event_EventType::Event_EventType_DELETE) {
            HandleDeleteEvent(event.kv().key(), event.prev_kv().value());
        }
    }
}

void EtcdService::RequestLease() {
    gNode->GetEtcdManager().RequestEtcdLease();
}

void EtcdService::KeepAlive() {
    gNode->GetEtcdManager().KeepNodeAlive();
}

void EtcdService::RegisterService() {
    gNode->GetEtcdManager().RegisterNodeService();
}

void EtcdService::Shutdown() {
    EtcdHelper::StopAllWatching();
    gNode->GetEtcdManager().Shutdown();
}

void EtcdService::OnLeaseGranted(const etcdserverpb::LeaseGrantResponse& reply) {
    if (leaseId <= 0) {
        leaseId = reply.id();
        NodeAllocator::AcquireNodePort();
        NodeAllocator::AcquireNode();
        KeepAlive();
    } else {
        leaseId = reply.id();
        KeepAlive();
        RegisterService();
    }
}
