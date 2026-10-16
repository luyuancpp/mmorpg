#pragma once

#include <string>
#include <unordered_map>
#include <chrono>
#include <deque>
#include <memory>
#include <vector>

#include "proto/etcd/etcd.pb.h"
#include "time/comp/timer_task_comp.h"
#include "node/system/discovery/discovery_key_index.h"
#include "node/system/etcd/etcd_transport.h"

// etcd 注册 / 发现 / 续租的状态机。应答经 EtcdResponseSink 从传输层回来(docs/design/grpc-muduo-transport.md §13.4);
// 两种传输共用这一套处理,只在流重建相关的部分分叉(RebuildsStreams())。
class EtcdService final : public EtcdResponseSink {
public:
    // 重试回调捕获 this;已武装的定时器可移动,宿主地址仍必须固定。
    EtcdService() = default;
    ~EtcdService() override = default;
    EtcdService(const EtcdService&) = delete;
    EtcdService& operator=(const EtcdService&) = delete;
    EtcdService(EtcdService&&) = delete;
    EtcdService& operator=(EtcdService&&) = delete;

    void Init();
    void StartWatchingPrefixes();
    void Shutdown();
    void RequestNodeLease();

    // EtcdHelper 的唯一出口。Init 之前 / Shutdown 之后返回 nullptr(调用方丢弃请求)。
    EtcdTransport* Transport() { return transport_.get(); }

    // 拿一个新 lease 并把端口 / 分配 / 服务三把 key 改挂上去,node_id 不变。
    // 触发源:健康监控发现本节点快照消失、keepalive 返回 TTL<=0、本地 ACK 超时。
    // 失租**不碰发号**(永久 guid 走号段,与 lease 无关):身份由 CAS 决定 —— 只有分配键的 value 已经是别人的
    // uuid(OnTxnFailed → OnNodeIdConflictShutdown)才是真的被抢。
    void RequestReRegistration(const char* reason);

    // 永久停掉注册流的所有重试(端口 / node_id / txn 超时兜底)。
    // 身份冲突收尾期间必须调用:这台节点已经不是 node_id 的合法持有者了,
    // 再去重抢端口 / 重占 node_id 只会干扰正在接手的那个进程。
    void StopRegistrationRetries();
    void RegisterService();
    void StartLeaseKeepAlive();

    // 注册流 CAS 的响应超时。由 EtcdManager 在设置 / 取走 pendingTxnKey_ 时成对调用,
    // 维持不变量:**有 pending key ⟺ 超时定时器在跑**。
    //
    // 为什么需要:生成的 etcd grpc client 只在 `status.ok()` 时才调 handler,
    // RPC 失败时**什么都不做** —— 于是注册流会永久停在某一阶段(没 node_id、
    // 不发布服务发现、也不重试)。到期后重查权威(重发同一条幂等 CAS 链),
    // 不是"假设成功继续往下走"。
    void ArmTxnTimeout();
    void CancelTxnTimeout();

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

    // 本节点路由身份"当前确认有效"的只读判定,给要写跨进程放行凭证的调用方用
    // (scene 的断线释放标记 A1′,docs/design/cross-zone-scene-travel.md §12.6.4 M3)。
    // 比 IsLeasePresumablyExpired 严格,四条全部满足才为 true:
    //   ① lease 已授予(没授予 = 身份还没确认,不能当成"没过期");
    //   ② 不在重注册中:keepalive 回 TTL<=0 之后、分配键 CAS 裁定之前身份悬而未决,而重新授予 lease 时
    //      OnLeaseGranted 会把 ACK 时间置为 now —— 只看 ACK 会在这段窗口里误判有效;
    //   ③ 注册流没有被永久停掉(身份冲突收尾中);
    //   ④ 最近一次 keepalive ACK 距今 ≤ TTL/2(毫秒精度,给 etcd 侧过期留出余量)。
    // 线程模型同本类其余成员:只在本节点 loop 线程调用。
    bool IsIdentityConfirmedFresh() const;

    // EtcdResponseSink(只在节点 loop 线程回调)
    void OnRange(const etcdserverpb::RangeRequest* request, const etcdserverpb::RangeResponse& reply) override;
    void OnPut(const etcdserverpb::PutResponse& reply) override;
    void OnDeleteRange(const etcdserverpb::DeleteRangeResponse& reply) override;
    void OnTxn(const etcdserverpb::TxnResponse& reply) override;
    void OnLeaseGrant(const etcdserverpb::LeaseGrantResponse& reply) override;
    void OnWatch(const etcdserverpb::WatchResponse& reply) override;
    void OnKeepAlive(const etcdserverpb::LeaseKeepAliveResponse& reply) override;
    void OnWatchStreamRestarted() override;
    void OnKeepAliveStreamRestarted() override;

private:
    enum class RegistrationMode : uint8_t {
        kInitialBoot,
        kReRegisterExisting,
    };

    // 新传输(RebuildsStreams)上每个发现前缀的 watch 状态(§13.5)。
    struct PrefixWatch {
        bool createSent = false;   // 当前流上已发 create(等 created 应答或已建立)
        int64_t watchId = -1;      // etcd 分配的 watch_id;-1 = 还没收到 created
    };

    void HandlePutEvent(const std::string& key, const std::string& value);
    void HandleDeleteEvent(const std::string& key, const std::string& value);
    void OnLeaseGranted(const ::etcdserverpb::LeaseGrantResponse& reply);
    void OnKeepAliveResponse(const ::etcdserverpb::LeaseKeepAliveResponse& reply);
    void OnWatchResponse(const ::etcdserverpb::WatchResponse& response);

    // 旧传输:改前 KV.Range handler 的原样逻辑(每次应答把全部前缀的 revision 推进到 R+1,首个应答发全部 watch)。
    void OnLegacyRange(const etcdserverpb::RangeResponse& reply);
    // 新传输:一个前缀的 Range 快照 → 应用 PUT → 对账删除 → 该前缀在当前流上还没 watch 就从 R+1 建。
    void OnPrefixRange(const std::string& prefix, const etcdserverpb::RangeResponse& reply);
    // 新传输的 watch 应答:created 按发送顺序对回前缀;canceled(含 compact)只重建那一个前缀。
    void OnStreamWatchResponse(const etcdserverpb::WatchResponse& response);
    void ApplyWatchEvents(const etcdserverpb::WatchResponse& response);
    // 一批(一个 Range 快照 / 一个 watch 应答)里待摘的发现节点。整批应用完才摘,且只摘不再挂在任何发现键上的 uuid。
    struct PendingRemoval {
        std::string key;
        std::string value;   // 删除前的 NodeInfo JSON,交给 HandleServiceNodeStop
        std::string uuid;    // 空 = 解析不了,无法判断是否还挂在别的键上
        std::string reason;  // 日志用
    };
    // 一条发现键 PUT(来自 Range 快照或 watch):过时的丢弃;同一个键换了持有者(uuid 不同)时把旧持有者记进 removals。
    void ApplyDiscoveryPut(const std::string& key, const std::string& value, int64_t modRevision,
                           std::vector<PendingRemoval>& removals);
    void FlushPendingRemovals(std::vector<PendingRemoval>& removals);
    void ResetPrefixWatch(const std::string& prefix);
    static bool IsAllocationKey(const std::string& key);

    void OnTxnSucceeded(const std::string& key);
    void OnTxnFailed(const std::string& key);
    // Allocates the RPC port and retries with backoff while none is available.
    // Reuses acquirePortTimer so there is a single retry path for the port phase.
    void AcquirePortWithRetry();

    void OnTxnTimeout();
    // LeaseGrant 的 RPC 失败时生成的 client 不调 handler,leaseRequestInFlight_ 会永久卡住 ——
    // etcd 抖一下就再也拿不到 lease。到期清标记重发。
    void OnLeaseGrantTimeout();
    void SetRegistrationMode(RegistrationMode mode, const char* reason);
    const char* RegistrationModeName(RegistrationMode mode) const;
    bool IsNodePortKey(const std::string& key) const;
    bool IsNodeIdKey(const std::string& key) const;
    // True if `key` is the global allocation key for this node's
    // (node_type, node_id). Distinguishing it from the per-zone NodeIdKey
    // lets OnTxnSucceeded skip Snowflake activation on the alloc-only step
    // (we activate Snowflake only after the per-zone publish succeeds).
    bool IsNodeAllocationKey(const std::string& key) const;

    void ScheduleWatchReconnect();
private:
    bool hasSentWatch = false;
    bool hasSentRange = false;
    // = transport_->RebuildsStreams(),Init 时定下;true 走 §13.5 的按前缀 watch / 对账路径。
    bool streamAware_ = false;
    bool leaseRequestInFlight_ = false;
    bool registrationStopped_ = false;
    RegistrationMode registrationMode_ = RegistrationMode::kInitialBoot;
    int64_t leaseId = 0;
    int64_t leaseTtlSeconds_ = 0;
    std::chrono::steady_clock::time_point lastKeepAliveAckTime_{};
    std::unordered_map<std::string, int64_t> revision;
    // 新传输专用(§13.5):前缀 → watch 状态;已发 create、等 created 应答的前缀(etcd 按发送顺序回 created);
    // watch_id → 前缀;发现键索引(DELETE 不依赖 prev_kv、对账、丢过时条目)。
    std::unordered_map<std::string, PrefixWatch> prefixWatches_;
    std::deque<std::string> pendingWatchCreates_;
    std::unordered_map<int64_t, std::string> watchIdToPrefix_;
    DiscoveryKeyIndex discoveryKeys_;
    // 每个前缀已经应用过的最新 Range 快照的 revision:更旧的应答整条丢弃,watch 从它之后建(跨流重建保留)。
    std::unordered_map<std::string, int64_t> appliedSnapshotRevision_;
    // 驱动进程里**全部**官方 gRPC 客户端的 CQ(不只 etcd),两种 etcd 传输下都要跑;第 3 批删 CQ 时一并删。
    TimerTaskComp grpcHandlerTimer;
    TimerTaskComp acquireNodeTimer;
	TimerTaskComp acquirePortTimer;
	TimerTaskComp watchReconnectTimer;
	TimerTaskComp txnTimeoutTimer;
	TimerTaskComp leaseGrantTimeoutTimer_;
    // 放在最后:先于上面的状态析构(传输层的回调只经 weak_ptr 进来,析构后不再回调本对象)。
    std::unique_ptr<EtcdTransport> transport_;
};
