#pragma once

// muduo 驱动的 gRPC 服务端(h2c,只支持 unary):docs/design/grpc-muduo-transport.md §15。
//
// 线程:Create / 构造不碰 loop;listenAddress() 构造后不变,任何线程可读。其余公开成员(含析构与只读 getter)
// 只能在构造时给定的 loop 线程调用,一律 loop.assertInLoopThread()(muduo 的这个断言在 release 下同样生效,LOG_FATAL)。
// 不创建线程,不加锁,不构造任何官方 grpc 对象。
// 接入、HTTP/2 协议(ServerSession)、派发泵、截止定时器、排空状态机、统计全部跑在这个 loop 上。
//
// 所有权(AGENTS §11.7):本类是 muduo 层的连接所有者(TcpServer 的等价物),连接表里按值持有
// TcpConnectionPtr —— 进程里唯一的强持有者。应用层(ServerCall、生成代码、Node)只经 weak 触达:
//   * Acceptor 的新连接回调、连接的 close 回调、定时器、queueInLoop 闭包一律绑 weak_ptr<Server>;
//   * 连接的 message / writeComplete 回调与连接自己的定时器绑 weak_ptr<detail::ServerConnection>;
//   * close 回调里无论 Server 还在不在,都无条件 queueInLoop(connectDestroyed)(照抄 TcpServer::removeConnectionInLoop),
//     否则 TcpConnection 析构撞 TcpConnection.cc:71 的 assert(state_ == kDisconnected)。
// 建议负责人在 AGENTS §11.7 的参考实现列表里补上 muduo_grpc::Server 与 ServerCall(§15.4.1,需负责人同意)。
//
// 析构前置:在 loop 线程上,且不在 I/O 事件处理期间 —— 只在 queueInLoop 的任务里或 loop 外析构。
//   原因:Acceptor 的 channel 在 eventHandling_ 期间 remove 会撞上 EventLoop.cc removeChannel 的断言
//   (Windows 版的定时器回调也跑在 eventHandling_ 期间)。BeginShutdown 自己把 Acceptor 移进 queueInLoop 闭包销毁。
//
// 恰好一次(§15.7):所有完成源(handler 的 Finish*、截止定时器、对端 RST、连接移除、排空到期、句柄全部丢弃、
// ~Server)都只在 loop 线程,汇到唯一裁决点 CompleteCall:第一步在调用表里找记录,找到即 erase。
//
// 线上不变量(由 server_test 锁住):
//   * 服务端自己产生的 UNAVAILABLE,文案永远不等于 "battle_not_allocatable"(那是 battle 守护段的跨语言契约,
//     go/match 按状态码加文案精确匹配,据此跳过 DestroyBattle)。本类只产生 "server dropped the call without
//     completing it" 与 "server shutting down" 两种 UNAVAILABLE。
//   * 「业务可能已执行」的失败只用 DEADLINE_EXCEEDED / UNAVAILABLE / CANCELLED;业务开始之前的拒绝才用
//     INTERNAL / UNIMPLEMENTED / RESOURCE_EXHAUSTED / REFUSED_STREAM。例外见 §15.13:应答超过 4MiB 时回
//     RESOURCE_EXHAUSTED(Go 客户端收包时本来也报同一个码),并打 ERROR。

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "metadata.h"
#include "method_table.h"
#include "server_call.h"
#include "server_session.h"
#include "status.h"

// muduo 头放在标准库头之后(它的 Windows 适配头 #define 了 off_t / pid_t 等名字)。
#include "muduo/net/Callbacks.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/TimerId.h"

namespace muduo
{
namespace net
{
class Acceptor;
class EventLoop;
} // namespace net
} // namespace muduo

namespace muduo_grpc
{

namespace detail
{
class ServerConnection;
} // namespace detail

struct ServerOptions
{
    std::string name{"grpc"};                           // 只用于连接名与日志
    ServerSessionOptions session;
    size_t maxSendMessageBytes{4 * 1024 * 1024};
    size_t maxPendingOutputBytes{1024 * 1024};          // 输出缓冲超过就暂停从 nghttp2 取帧,writeComplete 后继续;
                                                        // 写半关之前的最终冲出不受它限制(见 ServerShutdownOptions::closeGrace)
    size_t maxInFlightCalls{1024};                      // 全节点在途(排队 + 已派发未完成);超限回 RESOURCE_EXHAUSTED
    size_t maxDispatchPerTurn{64};                      // 派发泵每轮最多派发几个(至少 1 个)
    std::chrono::microseconds dispatchTurnBudget{2000}; // 派发泵每轮累计耗时上限(至少派发 1 个)
    std::chrono::milliseconds handshakeTimeout{120000}; // accept 到收到首个非 ACK SETTINGS;0 = 不限
    std::chrono::seconds statsLogInterval{30};          // [GrpcServer] summary 的检查周期;0 = 不打汇总(单测用)
};

struct ServerShutdownOptions
{
    std::chrono::milliseconds drainTimeout{2000};       // 从 BeginShutdown 起算
    std::chrono::milliseconds goawayPingTimeout{1000};  // 第二段 GOAWAY:收到 PING ACK 或满这个时长,取先到者
    // 优雅关闭 = 先把 nghttp2 里已提交的帧全部交给输出缓冲(这一次不看 maxPendingOutputBytes),再写半关
    // (muduo 在输出缓冲写空之后才发 FIN);从写半关起算这个宽限,是硬上限:对端不读导致写不空、或对端迟迟不关,
    // 到期一律 forceClose。
    std::chrono::milliseconds closeGrace{1000};
};

// 纯值类型(§15.11)。计数从 Server 构造起单调累加;conns / inflight 是当前值。
// 维度只有方法(≤15 个)乘状态码(≤17 个),不出现 player_id 与 peer。
struct ServerStatsSnapshot
{
    std::string listen;
    uint64_t connections{0};
    uint64_t connectionsPeak{0};
    uint64_t accepted{0};
    uint64_t closedBeforePreface{0};     // 移除时一个字节都没收到过的连接(探针、握手超时、停机时直接关掉的)
    uint64_t handshakeTimeout{0};
    uint64_t protocolErrors{0};          // 坏 magic / 帧级协议错误 / 洪泛:连接级
    uint64_t inFlight{0};
    uint64_t inFlightPeak{0};
    uint64_t dispatched{0};
    uint64_t maxBacklog{0};              // 派发队列的历史最大长度
    uint64_t maxDispatchedPerTurn{0};
    uint64_t rejectedOverload{0};        // 全节点在途上限
    uint64_t rejectedProtocol{0};        // ServerSession 自己拒掉的请求(§15.5.3 本层判定的那些行;timeout <= 0 除外)
    uint64_t unimplemented{0};           // 未知服务 / 未知方法
    uint64_t deadlineBeforeStart{0};     // 业务开始前到期:grpc-timeout <= 0、排队中 / 等许可时到期
    uint64_t deadlineAfterStart{0};
    uint64_t cancelledBeforeStart{0};    // 对端 RST / 断连时业务还没开始
    uint64_t abortedAfterStart{0};       // 对端 RST / 断连时业务已开始
    uint64_t refusedOnShutdown{0};       // 排空到期回 REFUSED_STREAM(含到期之后才开 / 才收完的流)
    uint64_t unavailableOnShutdown{0};   // 排空到期时业务已开始,回 UNAVAILABLE "server shutting down"
    uint64_t lateFinish{0};              // 调用已收口之后的 Finish*
    uint64_t dropped{0};                 // 句柄全部丢弃仍未完成(程序缺陷信号)
    uint64_t droppedOffLoop{0};          // 进程级:句柄在 loop 线程之外析构
    std::map<uint32_t, uint64_t> codes;  // 回给对端的 grpc-status → 次数(REFUSED_STREAM 与静默撤销不计)
    std::map<std::string, std::map<uint32_t, uint64_t>> methods;   // 方法名 → (grpc-status → 次数)
};

// 一行汇总(不含 "[GrpcServer] summary " 前缀),字段顺序由单测锁住:
// listen= conns=<cur>/<peak> accepted= closed_before_preface= handshake_timeout= protocol_errors=
// inflight=<cur>/<peak> dispatched= max_backlog= max_dispatched_per_turn= rejected_overload= rejected_protocol=
// unimplemented= deadline_before_start= deadline_after_start= cancelled_before_start= aborted_after_start=
// refused_on_shutdown= unavailable_on_shutdown= late_finish= dropped= dropped_off_loop= codes=<code:n,...>
// methods={<Method>:<code>=<n>,...}
std::string FormatServerStats(const ServerStatsSnapshot &stats);

class Server : public std::enable_shared_from_this<Server>
{
    struct PrivateTag
    {
        explicit PrivateTag() = default;
    };

public:
    // ipv4 不是规范 IPv4 字面量,或 port == 0:LOG_ERROR 后返回 nullptr(调用方按 §15.3 Q8 决定是否 LOG_FATAL)。
    // 绑这个地址,不开 SO_REUSEPORT。
    static std::shared_ptr<Server> Create(muduo::net::EventLoop &loop, const std::string &ipv4, uint16_t port,
                                          ServerOptions options = {});

    Server(PrivateTag, muduo::net::EventLoop &loop, const muduo::net::InetAddress &listenAddress,
           ServerOptions options);
    // 取消所有定时器;剩余调用静默收口;剩余连接逐条 forceClose(排进的两趟由节点的两跳 quit 覆盖);
    // 不再投递 onDrained(排空完成时已经投进队列的那一次照常执行,调用方的回调须容忍服务端已销毁)。
    ~Server();
    Server(const Server &) = delete;
    Server &operator=(const Server &) = delete;

    // 只能在 Start 之前;路径非法或重复 → false(此后 Start 拒绝启动)。
    bool RegisterService(Service &service);
    // 建 Acceptor(bind 失败按 muduo LOG_SYSFATAL)并 listen;只调一次。方法表非法 → LOG_FATAL。
    void Start();
    // 幂等(重复调用只打 WARN 并忽略,第二次传入的 onDrained 不会被调用)。
    // onDrained 恰好调用一次,一律经 queueInLoop 投递,永不在本调用栈内执行;可以从 handler 内调用。
    // 内部顺序(§15.10):a 置排空;b Acceptor 移进 queueInLoop 闭包销毁(停听,销毁前到达的接入一律 close);
    // c 每条连接:收到过字节 → GOAWAY(2^31-1) + PING,上弦 goawayPingTimeout;没收到过 → forceClose;
    // d 上弦 drainTimeout 排空定时器(steady 复核);e 已满足排空条件 → queueInLoop(onDrained)。
    void BeginShutdown(ServerShutdownOptions options, std::function<void()> onDrained);

    // 以下四个读的是 loop 独占的可变状态:同样断言在 loop 线程(见文件头「线程」)。
    bool draining() const;
    size_t inFlightCalls() const;
    size_t connectionCount() const;
    ServerStatsSnapshot Stats() const;
    // 构造后不变:任何线程可读,不断言。
    const std::string &listenAddress() const { return listenAddress_; }

private:
    friend class ServerCall;
    friend struct detail::CallToken;
    friend class detail::ServerConnection;

    enum class Phase : uint8_t
    {
        kCreated,
        kRunning,
        kDraining,       // BeginShutdown 之后:已接受的流照常处理
        kDrainExpired,   // 排空到期:新流一律 REFUSED_STREAM
        kDrained,        // 连接表已空,onDrained 已投递
    };

    enum class CallPhase : uint8_t
    {
        kQueued,       // 已登记,等派发泵
        kDispatched,   // handler 已拿到句柄,业务尚未开始(例如在等 Agones 许可)
        kStarted,      // TryStartBusiness 为真之后
    };

    enum class CallEndKind : uint8_t
    {
        kResponse,   // HEADERS + DATA + trailers(grpc-status 0)
        kStatus,     // 非 OK 的 trailers-only
        kRefuse,     // RST_STREAM(REFUSED_STREAM):告诉对端没处理
        kSilent,     // 对端已走(RST / 断连),什么都不发
    };

    struct CallEnd
    {
        CallEndKind kind{CallEndKind::kSilent};
        std::string response;   // kResponse
        Status status;          // kStatus(非 OK)
    };

    // 调用表里的一条记录。没有任何 T* 字段。
    struct CallRecord
    {
        uint64_t connectionId{0};
        int32_t streamId{0};
        size_t methodIndex{0};
        std::string peer;
        Metadata metadata;                                              // 派发时移进 CallToken
        std::optional<std::chrono::steady_clock::time_point> deadline;
        std::string message;                                            // 派发时移出(handler 可能同步完成并删掉记录)
        CallPhase phase{CallPhase::kQueued};
    };

    struct MethodEntry
    {
        std::string path;
        std::string displayName;   // 统计与日志用:方法名;两个服务同名时退回完整路径
        UnaryMethodHandler handler;
    };

    struct ConnectionEntry
    {
        muduo::net::TcpConnectionPtr tcp;                        // 进程里唯一的强持有(§11.7)
        std::shared_ptr<detail::ServerConnection> connection;
    };

    // 单线程计数(Stats 时拷成快照)。
    struct Counters
    {
        uint64_t connectionsPeak{0};
        uint64_t accepted{0};
        uint64_t closedBeforePreface{0};
        uint64_t handshakeTimeout{0};
        uint64_t protocolErrors{0};
        uint64_t inFlightPeak{0};
        uint64_t dispatched{0};
        uint64_t maxBacklog{0};
        uint64_t maxDispatchedPerTurn{0};
        uint64_t rejectedOverload{0};
        uint64_t rejectedProtocol{0};
        uint64_t unimplemented{0};
        uint64_t deadlineBeforeStart{0};
        uint64_t deadlineAfterStart{0};
        uint64_t cancelledBeforeStart{0};
        uint64_t abortedAfterStart{0};
        uint64_t refusedOnShutdown{0};
        uint64_t unavailableOnShutdown{0};
        uint64_t lateFinish{0};
        uint64_t dropped{0};
        std::map<uint32_t, uint64_t> codes;
        std::vector<std::map<uint32_t, uint64_t>> methodCodes;   // 下标同 methods_
    };

    struct StringHash
    {
        using is_transparent = void;
        size_t operator()(std::string_view value) const { return std::hash<std::string_view>{}(value); }
    };

    // 作用域内产生的应答只登记「连接需要冲出」,作用域结束时统一冲出(派发泵、截止 / 排空定时器、BeginShutdown)。
    class BatchScope;

    // ---- 接入与连接(实现在 server.cpp)----
    void HandleNewConnection(int sockfd, const muduo::net::InetAddress &peerAddress);
    // close 回调里调用:摘表、撤销该连接上的全部调用、取消连接定时器;返回被摘下的连接,由调用方移进
    // connectDestroyed 闭包一起释放(不在它自己的回调栈上析构)。
    std::shared_ptr<detail::ServerConnection> DetachConnection(uint64_t connectionId);
    void MarkConnectionDirty(uint64_t connectionId);
    void FlushDirtyConnections();

    // ---- ServerConnection(ServerSession::Observer)转来的事件:都在 nghttp2 回调栈里,只查表、登记、入队 ----
    ServerSession::Verdict RouteRequest(const RequestHeaders &headers);
    ServerSession::Verdict AcceptCall(detail::ServerConnection &connection, int32_t streamId, RequestHeaders headers,
                                      std::string message, std::chrono::steady_clock::time_point headersAt);
    void AbortStream(detail::ServerConnection &connection, int32_t streamId);
    void OnSessionRejected(const detail::ServerConnection &connection, int httpStatus, const Status &status);
    void OnConnectionProtocolError(const detail::ServerConnection &connection, const Status &status);
    void OnHandshakeTimeout(const detail::ServerConnection &connection);

    // ---- 派发 ----
    void ScheduleDispatch();
    void RunDispatchTurn();

    // ---- 恰好一次 ----
    // 唯一裁决点:记录在 → 移出并 erase、递减在途、提交应答、标记连接需要冲出,返回完成前的阶段;不在 → nullopt。
    std::optional<CallPhase> CompleteCall(uint64_t callId, CallEnd end);
    bool TryStartBusiness(uint64_t callId);
    void FinishFromHandler(const detail::CallToken &token, CallEnd end);
    void OnTokenDropped(const detail::CallToken &token);
    void CountStatus(std::optional<size_t> methodIndex, StatusCode code);

    // ---- 定时器 ----
    void ArmDeadlineTimer();
    void OnDeadlineTimer();
    void ArmDrainTimer();
    void OnDrainTimer();
    void MaybeFinishDrain();
    void LogStatsIfChanged();
    void CancelTimers();

    muduo::net::EventLoop &loop_;
    const muduo::net::InetAddress listenInetAddress_;
    const std::string listenAddress_;
    const ServerOptions options_;

    MethodTable methodTable_;                                                  // Start 之前登记;之后只读
    std::vector<MethodEntry> methods_;                                         // Start 时冻结,下标即 methodIndex
    std::unordered_map<std::string, size_t, StringHash, std::equal_to<>> methodIndex_;

    std::shared_ptr<muduo::net::Acceptor> acceptor_;   // shared_ptr:BeginShutdown 把它移进 queueInLoop 闭包销毁
    std::map<uint64_t, ConnectionEntry> connections_;
    uint64_t nextConnectionId_{1};

    std::unordered_map<uint64_t, CallRecord> calls_;
    uint64_t nextCallId_{1};
    std::deque<uint64_t> ready_;   // 等派发的 callId(先进先出;已被撤销的残留 id 派发时跳过)
    std::set<std::pair<std::chrono::steady_clock::time_point, uint64_t>> deadlines_;
    bool dispatchQueued_{false};

    int batchDepth_{0};
    std::set<uint64_t> dirtyConnections_;

    Phase phase_{Phase::kCreated};
    ServerShutdownOptions shutdownOptions_;
    std::function<void()> onDrained_;
    std::chrono::steady_clock::time_point drainStartedAt_{};
    std::chrono::steady_clock::time_point drainDeadline_{};

    // muduo 定时器按墙钟走,只当唤醒提示;到点按 steady_clock 复核,未到就按剩余时间重新上弦。
    muduo::net::TimerId deadlineTimer_;
    bool deadlineTimerArmed_{false};
    std::chrono::steady_clock::time_point deadlineTimerFor_{};
    muduo::net::TimerId drainTimer_;
    bool drainTimerArmed_{false};
    muduo::net::TimerId statsTimer_;
    bool statsTimerArmed_{false};
    std::string lastLoggedStats_;

    Counters stats_;
};

} // namespace muduo_grpc
