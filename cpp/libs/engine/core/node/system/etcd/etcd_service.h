#pragma once

#include <string>
#include <deque>
#include <unordered_map>
#include <unordered_set>

#include "proto/etcd/etcd.pb.h"
#include "proto/common/event/node_event.pb.h"
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

	int64_t GetLeaseId() const { return leaseId; }

private:
    enum class RegistrationMode : uint8_t {
        kInitialBoot,
        kReRegisterExisting,
    };

    void HandlePutEvent(const std::string& key, const std::string& value);
    void HandleDeleteEvent(const std::string& key, const std::string& value);
    void OnLeaseGranted(const ::etcdserverpb::LeaseGrantResponse& reply);
    void OnWatchResponse(const ::etcdserverpb::WatchResponse& response);
    void OnTxnSucceeded(const std::string& key);
    void OnTxnFailed(const std::string& key);
    void SetRegistrationMode(RegistrationMode mode, const char* reason);
    const char* RegistrationModeName(RegistrationMode mode) const;
    bool IsNodePortKey(const std::string& key) const;
    bool IsNodeIdKey(const std::string& key) const;

    void InitKVHandlers();
    void InitWatchHandlers();
    void InitLeaseHandlers();
    void InitTxnHandlers();
private:
    bool hasSentWatch = false;
    bool hasSentRange = false;
    bool leaseRequestInFlight_ = false;
    RegistrationMode registrationMode_ = RegistrationMode::kInitialBoot;
    int64_t leaseId = 0;
    std::unordered_map<std::string, int64_t> revision;
    TimerTaskComp grpcHandlerTimer;
    TimerTaskComp acquireNodeTimer;
	TimerTaskComp acquirePortTimer;
};
