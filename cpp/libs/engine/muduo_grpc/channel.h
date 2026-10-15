#pragma once

// muduo 驱动的 gRPC 客户端通道:一个目标地址(ip:port)一个对象(docs/design/grpc-muduo-transport.md)。
//
// 对调用方的契约:
//   * 只能在构造时给定的 EventLoop 线程上使用(断言)。本模块不创建任何线程、线程池或 CQ。
//   * StartUnaryCall 的完成回调**恰好一次**,并且**永不**在 StartUnaryCall 内同步执行(一律 queueInLoop 投递)。
//   * 回调里的 Status 只有 OK 才代表对端完成了业务;DEADLINE_EXCEEDED / UNAVAILABLE / CANCELLED 等一律是
//     「结果未知」—— 对端可能已经执行。本模块只对可证明未被处理的请求做一次透明重试(§4.6),不做别的重发。
//   * 通道析构时,未完成的调用以 UNAVAILABLE("channel shut down")投递回调;回调不能指望通道还活着。
//
// 生命周期(AGENTS.md §11.7):必须由 shared_ptr 持有(Create);定时器与连接回调只绑 weak_ptr<自己>;
// 通道必须在其 EventLoop 仍在运行时、于 loop 线程上析构(节点关机时 Node::FinalizeShutdownInLoop 负责清理)。

#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "muduo/net/InetAddress.h"
#include "muduo/net/TimerId.h"

#include "http2_transport.h"
#include "metadata.h"
#include "status.h"

namespace muduo
{
namespace net
{
class EventLoop;
} // namespace net
} // namespace muduo

namespace muduo_grpc
{

// 与 gRPC 的 connectivity state 同名同义(§5)。
enum class ConnectivityState : uint8_t
{
    kIdle,
    kConnecting,
    kReady,
    kTransientFailure,
    kShutdown,
};

const char *ConnectivityStateName(ConnectivityState state);

// gRPC doc/connection-backoff.md 的默认值。
struct BackoffOptions
{
    std::chrono::milliseconds initial{1000};
    double multiplier{1.6};
    double jitter{0.2};
    std::chrono::milliseconds max{120000};
};

struct ChannelOptions
{
    std::string authority;                              // 空 = "ip:port"
    std::string userAgent{"grpc-c++-muduo/1.0"};
    size_t maxSendMessageBytes{4 * 1024 * 1024};        // grpc-go 服务端默认接收上限
    size_t maxReceiveMessageBytes{4 * 1024 * 1024};     // grpc C++ 默认接收上限
    uint32_t maxHeaderListBytes{16 * 1024};
    uint32_t initialStreamWindowBytes{1024 * 1024};
    uint32_t connectionWindowBytes{16 * 1024 * 1024};
    size_t maxOutstandingCalls{65536};                  // 排队 + 活跃;超出本地 RESOURCE_EXHAUSTED
    size_t maxPendingOutputBytes{1024 * 1024};
    std::chrono::milliseconds connectTimeout{20000};
    std::chrono::milliseconds keepaliveTimeout{20000};
    BackoffOptions backoff;
};

struct UnaryCallRequest
{
    std::string method;    // ":path",例如 "/client_rpc_router.ClientRpcRouter/Forward"
    std::string message;   // 已序列化的请求 protobuf
    Metadata metadata;     // 应用层值;-bin 键由传输层再编一层 base64
    std::chrono::steady_clock::time_point deadline;  // 必填;线上编码成 grpc-timeout
};

struct UnaryCallResult
{
    Status status;
    Metadata initialMetadata;   // 响应头里的自定义 metadata(-bin 已解掉线上那层);失败时可能为空
    Metadata trailingMetadata;
    std::string response;       // status OK 时是响应 protobuf 的字节
};

using UnaryCallback = std::function<void(UnaryCallResult &&result)>;

class Channel : public std::enable_shared_from_this<Channel>, public Http2TransportListener
{
    struct PrivateTag
    {
        explicit PrivateTag() = default;
    };

public:
    // ip 只接受 IPv4 字面量(节点注册的 grpc_endpoint 都是 POD / 宿主 IP);非法返回 nullptr 并记 ERROR。
    static std::shared_ptr<Channel> Create(muduo::net::EventLoop &loop, const std::string &ip, uint16_t port,
                                           ChannelOptions options = {});

    Channel(PrivateTag, muduo::net::EventLoop &loop, const std::string &ip, uint16_t port, ChannelOptions options);
    ~Channel() override;
    Channel(const Channel &) = delete;
    Channel &operator=(const Channel &) = delete;

    void StartUnaryCall(UnaryCallRequest request, UnaryCallback done);

    // 与 grpc::Channel::GetState 同义:tryToConnect 且处于 IDLE 时开始连接。
    ConnectivityState GetState(bool tryToConnect);

    const std::string &target() const { return target_; }
    size_t outstandingCalls() const { return calls_.size(); }

private:
    struct Call
    {
        uint64_t id{0};
        std::string path;
        std::shared_ptr<const std::string> framedMessage;
        Metadata metadata;
        std::chrono::steady_clock::time_point deadline;
        UnaryCallback done;
        bool active{false};          // 已在某条连接上开了流
        uint64_t transportId{0};
        int32_t streamId{0};
        std::chrono::steady_clock::time_point startedAt{};
        bool transparentRetried{false};
    };

    // Http2TransportListener
    void OnTransportReady(Http2Transport &transport) override;
    void OnTransportGoaway(Http2Transport &transport, int32_t lastStreamId, uint32_t errorCode,
                           std::string_view debugData) override;
    void OnTransportStreamClosed(Http2Transport &transport, int32_t streamId, StreamOutcome outcome) override;
    void OnTransportClosed(Http2Transport &transport, const Status &reason) override;

    void Dispatch(uint64_t callId);
    void StartOnCurrentTransport(Call &call);
    void FlushPending();
    void QueueFlushPending();
    void QueueRedispatch(uint64_t callId);
    void CompleteCall(uint64_t callId, UnaryCallResult result);
    void FailSoon(UnaryCallback done, Status status);
    void ExpireCall(uint64_t callId);

    void StartConnecting();
    void CreateTransport();
    void RetireCurrentTransport();
    void EnterTransientFailure(const Status &reason);
    void ScheduleReconnect();
    void OnBackoffTimer();
    void ReleaseTransport(std::shared_ptr<Http2Transport> transport);
    void MaybeReleaseDrainingTransport(Http2Transport &transport);
    std::shared_ptr<Http2Transport> FindTransport(uint64_t transportId) const;

    void ArmDeadlineTimer();
    void OnDeadlineTimer();
    void SetState(ConnectivityState state);
    bool LoopIsCurrent() const;

    muduo::net::EventLoop &loop_;
    const std::string target_;
    const muduo::net::InetAddress address_;
    const ChannelOptions options_;
    Http2TransportOptions transportOptions_;

    ConnectivityState state_{ConnectivityState::kIdle};
    Status lastFailure_;

    std::shared_ptr<Http2Transport> current_;                 // 正在连接或 READY 的连接
    std::vector<std::shared_ptr<Http2Transport>> draining_;   // GOAWAY 后等在途流结束的连接
    uint64_t nextTransportId_{1};

    uint64_t nextCallId_{1};
    std::unordered_map<uint64_t, Call> calls_;                // 所有未完成的调用
    std::deque<uint64_t> pending_;                            // 等连接 READY 的调用(可能含已完成的残留 id)
    std::set<std::pair<std::chrono::steady_clock::time_point, uint64_t>> deadlines_;
    std::map<std::pair<uint64_t, int32_t>, uint64_t> streamToCall_;  // (连接, 流) → 调用
    bool flushPendingQueued_{false};

    muduo::net::TimerId deadlineTimer_;
    bool deadlineTimerArmed_{false};
    std::chrono::steady_clock::time_point deadlineTimerFor_{};

    muduo::net::TimerId backoffTimer_;
    bool backoffTimerArmed_{false};
    double nextBackoffMs_;
    std::mt19937 rng_;
};

} // namespace muduo_grpc
