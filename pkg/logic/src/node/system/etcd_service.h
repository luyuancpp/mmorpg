#pragma once

#include <string>
#include <deque>
#include <unordered_map>
#include <unordered_set>

#include "etcd/etcd.pb.h"
#include "proto/logic/event/node_event.pb.h"
#include "time/comp/timer_task_comp.h"

class EtcdService {
public:
    void Init();
    void InitHandlers();
    void StartWatchingPrefixes();
    void Shutdown();
    void RequestLease();
    void RegisterService();
    void KeepAlive();

private:
    void HandlePutEvent(const std::string& key, const std::string& value);
    void HandleDeleteEvent(const std::string& key, const std::string& value);
    void OnLeaseGranted(const ::etcdserverpb::LeaseGrantResponse& reply);
    void OnWatchResponse(const ::etcdserverpb::WatchResponse& response);

    void InitKVHandlers();
    void InitWatchHandlers();
    void InitLeaseHandlers();
    void InitTxnHandlers();
private:
    bool hasSentWatch = false;
    bool hasSentRange = false;
    int64_t leaseId = 0;
    std::deque<std::string> pendingKeys;
    std::unordered_map<std::string, int64_t> revision;
    TimerTaskComp grpcHandlerTimer;
    TimerTaskComp acquireNodeTimer;
	TimerTaskComp acquirePortTimer;
};
