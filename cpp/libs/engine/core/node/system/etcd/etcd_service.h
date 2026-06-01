#pragma once

#include <string>
#include <unordered_map>
#include <chrono>

#include "proto/etcd/etcd.pb.h"
#include "time/comp/timer_task_comp.h"

class EtcdService {
public:
    void Init();
    void InitHandlers();
    void StartWatchingPrefixes();
    void Shutdown();
    void RequestNodeLease();
    void RequestReRegistration();
    void RegisterService();
    void StartLeaseKeepAlive();

    // Called by Node::StartGrpcServer after the gRPC port is bound and accepting
    // connections. Publishes the discovery key (NodeInfo) that peers watch.
    // The publish was deliberately deferred from OnTxnSucceeded(allocKey) so
    // peers don't see the node alive via etcd watch and dial gRPC before the
    // port is open (cold-start "connection refused" race).
    // No-op for re-registration mode (gRPC stayed up, publish happened early).
    void PublishDiscoveryAfterGrpcReady();

    int64_t GetLeaseId() const { return leaseId; }

    // Returns true if we haven't received a keepalive ACK within the lease TTL,
    // meaning etcd has likely expired our lease and another node could claim our ID.
    bool IsLeasePresumablyExpired() const;

private:
    enum class RegistrationMode : uint8_t {
        kInitialBoot,
        kReRegisterExisting,
    };

    void HandlePutEvent(const std::string& key, const std::string& value);
    void HandleDeleteEvent(const std::string& key, const std::string& value);
    void OnLeaseGranted(const ::etcdserverpb::LeaseGrantResponse& reply);
    void OnKeepAliveResponse(const ::etcdserverpb::LeaseKeepAliveResponse& reply);
    void OnWatchResponse(const ::etcdserverpb::WatchResponse& response);
    void OnTxnSucceeded(const std::string& key);
    void OnTxnFailed(const std::string& key);
    void ActivateSnowFlakeAfterGuard();
    void SetRegistrationMode(RegistrationMode mode, const char* reason);
    const char* RegistrationModeName(RegistrationMode mode) const;
    bool IsNodePortKey(const std::string& key) const;
    bool IsNodeIdKey(const std::string& key) const;
    // True if `key` is the global allocation key for this node's
    // (node_type, node_id). Distinguishing it from the per-zone NodeIdKey
    // lets OnTxnSucceeded skip Snowflake activation on the alloc-only step
    // (we activate Snowflake only after the per-zone publish succeeds).
    bool IsNodeAllocationKey(const std::string& key) const;

    void InitKVHandlers();
    void InitWatchHandlers();
    void InitLeaseHandlers();
    void InitTxnHandlers();
    void ScheduleWatchReconnect();
private:
    bool hasSentWatch = false;
    bool hasSentRange = false;
    bool leaseRequestInFlight_ = false;
    RegistrationMode registrationMode_ = RegistrationMode::kInitialBoot;
    int64_t leaseId = 0;
    int64_t leaseTtlSeconds_ = 0;
    std::chrono::steady_clock::time_point lastKeepAliveAckTime_{};
    std::unordered_map<std::string, int64_t> revision;
    TimerTaskComp grpcHandlerTimer;
    TimerTaskComp acquireNodeTimer;
	TimerTaskComp acquirePortTimer;
	TimerTaskComp watchReconnectTimer;
};
