#include "channel.h"

#include <algorithm>
#include <charconv>

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"

#include "grpc_timeout.h"
#include "message_framing.h"

namespace muduo_grpc
{

namespace
{

constexpr uint32_t kHttp2Cancel = 0x8;

// 只接受规范的点分十进制 IPv4(不接受前导零、空段、主机名)。muduo 的 InetAddress 遇到非法 IP
// 只打一行日志并留下 0.0.0.0 —— 在 Linux 上那会连到本机,所以必须先校验。
bool IsCanonicalIpv4(const std::string &ip)
{
    int parts = 0;
    size_t pos = 0;
    while (pos <= ip.size())
    {
        const size_t dot = ip.find('.', pos);
        const size_t end = dot == std::string::npos ? ip.size() : dot;
        const size_t len = end - pos;
        if (len == 0 || len > 3 || (len > 1 && ip[pos] == '0'))
        {
            return false;
        }
        int value = 0;
        const auto [ptr, ec] = std::from_chars(ip.data() + pos, ip.data() + end, value);
        if (ec != std::errc{} || ptr != ip.data() + end || value > 255)
        {
            return false;
        }
        ++parts;
        if (dot == std::string::npos)
        {
            break;
        }
        pos = dot + 1;
    }
    return parts == 4;
}

double ToSeconds(std::chrono::steady_clock::duration duration)
{
    return std::max(0.0, std::chrono::duration<double>(duration).count());
}

UnaryCallResult FailureResult(Status status)
{
    UnaryCallResult result;
    result.status = std::move(status);
    return result;
}

} // namespace

const char *ConnectivityStateName(ConnectivityState state)
{
    switch (state)
    {
    case ConnectivityState::kIdle: return "IDLE";
    case ConnectivityState::kConnecting: return "CONNECTING";
    case ConnectivityState::kReady: return "READY";
    case ConnectivityState::kTransientFailure: return "TRANSIENT_FAILURE";
    case ConnectivityState::kShutdown: return "SHUTDOWN";
    }
    return "UNKNOWN";
}

std::shared_ptr<Channel> Channel::Create(muduo::net::EventLoop &loop, const std::string &ip, uint16_t port,
                                         ChannelOptions options)
{
    if (!IsCanonicalIpv4(ip) || port == 0)
    {
        LOG_ERROR << "muduo_grpc::Channel rejects target '" << ip << ":" << port
                  << "': only IPv4 literals with a non-zero port are supported";
        return nullptr;
    }
    return std::make_shared<Channel>(PrivateTag{}, loop, ip, port, std::move(options));
}

Channel::Channel(PrivateTag, muduo::net::EventLoop &loop, const std::string &ip, uint16_t port,
                 ChannelOptions options)
    : loop_(loop),
      target_(ip + ":" + std::to_string(port)),
      address_(ip, port),
      options_(std::move(options)),
      nextBackoffMs_(static_cast<double>(options_.backoff.initial.count())),
      rng_(std::random_device{}())
{
    transportOptions_.session.authority = options_.authority.empty() ? target_ : options_.authority;
    transportOptions_.session.userAgent = options_.userAgent;
    transportOptions_.session.maxReceiveMessageBytes = options_.maxReceiveMessageBytes;
    transportOptions_.session.maxHeaderListBytes = options_.maxHeaderListBytes;
    transportOptions_.session.initialStreamWindowBytes = options_.initialStreamWindowBytes;
    transportOptions_.session.connectionWindowBytes = options_.connectionWindowBytes;
    transportOptions_.maxPendingOutputBytes = options_.maxPendingOutputBytes;
    transportOptions_.connectTimeout = options_.connectTimeout;
    transportOptions_.keepaliveTimeout = options_.keepaliveTimeout;
}

Channel::~Channel()
{
    state_ = ConnectivityState::kShutdown;
    const bool loopIsCurrent = LoopIsCurrent();
    if (!loopIsCurrent)
    {
        // 正常关机路径(Node::FinalizeShutdownInLoop)一定在 loop 线程上、loop 仍在运行时析构;走到这里说明有人漏了清理。
        LOG_ERROR << "muduo_grpc::Channel " << target_
                  << " destroyed outside its EventLoop thread; pending callbacks are dropped";
    }
    else
    {
        if (deadlineTimerArmed_)
        {
            loop_.cancel(deadlineTimer_);
        }
        if (backoffTimerArmed_)
        {
            loop_.cancel(backoffTimer_);
        }
        // 未完成的调用:回调不捕获通道,通道析构后照样安全执行。
        for (auto &[callId, call] : calls_)
        {
            loop_.queueInLoop([done = std::move(call.done)]() mutable
                              { done(FailureResult(Status(StatusCode::kUnavailable, "channel shut down"))); });
        }
    }
    calls_.clear();
    pending_.clear();
    deadlines_.clear();
    streamToCall_.clear();
    if (current_)
    {
        current_->Shutdown();
    }
    for (const auto &transport : draining_)
    {
        transport->Shutdown();
    }
    // 连接对象在这里析构:通道析构只会发生在排队的任务或节点关机流程里,不在任何连接自己的回调栈上。
}

void Channel::StartUnaryCall(UnaryCallRequest request, UnaryCallback done)
{
    loop_.assertInLoopThread();
    if (!done)
    {
        LOG_ERROR << "muduo_grpc::Channel::StartUnaryCall called without a completion callback, method="
                  << request.method;
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (state_ == ConnectivityState::kShutdown)
    {
        FailSoon(std::move(done), Status(StatusCode::kUnavailable, "channel shut down"));
        return;
    }
    if (calls_.size() >= options_.maxOutstandingCalls)
    {
        FailSoon(std::move(done), Status(StatusCode::kResourceExhausted,
                                         "too many outstanding calls on channel " + target_ + " (limit " +
                                             std::to_string(options_.maxOutstandingCalls) + ")"));
        return;
    }
    if (request.message.size() > options_.maxSendMessageBytes)
    {
        FailSoon(std::move(done), Status(StatusCode::kResourceExhausted,
                                         "sent message larger than max (" + std::to_string(request.message.size()) +
                                             " vs. " + std::to_string(options_.maxSendMessageBytes) + ")"));
        return;
    }
    for (const auto &[key, value] : request.metadata)
    {
        if (!IsValidCustomMetadataKey(key) || (!IsBinaryHeaderKey(key) && !IsValidAsciiMetadataValue(value)))
        {
            FailSoon(std::move(done), Status(StatusCode::kInternal, "invalid metadata '" + key + "'"));
            return;
        }
    }
    if (request.deadline <= now)
    {
        FailSoon(std::move(done), Status(StatusCode::kDeadlineExceeded, "deadline exceeded before the call started"));
        return;
    }

    auto framed = std::make_shared<std::string>();
    AppendLengthPrefixedMessage(*framed, request.message);

    const uint64_t callId = nextCallId_++;
    Call call;
    call.id = callId;
    call.path = std::move(request.method);
    call.framedMessage = std::move(framed);
    call.metadata = std::move(request.metadata);
    call.deadline = request.deadline;
    call.done = std::move(done);
    calls_.emplace(callId, std::move(call));
    deadlines_.emplace(request.deadline, callId);
    ArmDeadlineTimer();
    Dispatch(callId);
}

ConnectivityState Channel::GetState(bool tryToConnect)
{
    loop_.assertInLoopThread();
    if (tryToConnect && state_ == ConnectivityState::kIdle)
    {
        StartConnecting();
    }
    return state_;
}

void Channel::Dispatch(uint64_t callId)
{
    const auto it = calls_.find(callId);
    if (it == calls_.end() || it->second.active)
    {
        return;
    }
    switch (state_)
    {
    case ConnectivityState::kReady:
        StartOnCurrentTransport(it->second);
        break;
    case ConnectivityState::kIdle:
        pending_.push_back(callId);
        StartConnecting();
        break;
    case ConnectivityState::kConnecting:
        pending_.push_back(callId);
        break;
    case ConnectivityState::kTransientFailure:
        // 未开 wait_for_ready:与现网官方客户端一致,立即失败。
        CompleteCall(callId, FailureResult(Status(StatusCode::kUnavailable,
                                                  "failed to connect to " + target_ + ": " + lastFailure_.message())));
        break;
    case ConnectivityState::kShutdown:
        CompleteCall(callId, FailureResult(Status(StatusCode::kUnavailable, "channel shut down")));
        break;
    }
}

void Channel::StartOnCurrentTransport(Call &call)
{
    if (!current_ || !current_->ready() || current_->draining())
    {
        // 状态与连接对不上(不应发生):当作连接已退役,重新排队并换连接。
        pending_.push_back(call.id);
        RetireCurrentTransport();
        return;
    }
    UnaryRequestSpec spec;
    spec.path = call.path;
    spec.framedMessage = call.framedMessage;
    spec.metadata = call.metadata;
    spec.grpcTimeout = EncodeGrpcTimeout(
        std::chrono::duration_cast<std::chrono::nanoseconds>(call.deadline - std::chrono::steady_clock::now()));

    const std::shared_ptr<Http2Transport> transport = current_;
    const auto result = transport->StartUnaryStream(spec);
    if (result.streamId > 0)
    {
        call.active = true;
        call.transportId = transport->id();
        call.streamId = result.streamId;
        call.startedAt = std::chrono::steady_clock::now();
        // 先登记再发送:发送过程中同步到达的流结束事件要能找到这次调用。
        streamToCall_[{call.transportId, call.streamId}] = call.id;
        transport->FlushOutput();
        return;
    }
    if (result.retryOnNewConnection)
    {
        // 请求没有发出(stream id 用尽 / 连接已 GOAWAY):换新连接重排队是安全的。
        const uint64_t callId = call.id;
        RetireCurrentTransport();
        pending_.push_back(callId);
        if (state_ == ConnectivityState::kIdle)
        {
            StartConnecting();
        }
        return;
    }
    CompleteCall(call.id, FailureResult(result.error));
}

void Channel::FlushPending()
{
    flushPendingQueued_ = false;
    if (state_ != ConnectivityState::kReady)
    {
        return;
    }
    std::deque<uint64_t> pending;
    pending.swap(pending_);
    for (const uint64_t callId : pending)
    {
        Dispatch(callId); // 已完成的残留 id 在 Dispatch 里按 calls_ 查不到而跳过
    }
}

void Channel::QueueFlushPending()
{
    if (flushPendingQueued_)
    {
        return;
    }
    flushPendingQueued_ = true;
    std::weak_ptr<Channel> weakSelf = weak_from_this();
    loop_.queueInLoop([weakSelf]
                      {
        if (const auto self = weakSelf.lock())
        {
            self->FlushPending();
        } });
}

void Channel::QueueRedispatch(uint64_t callId)
{
    std::weak_ptr<Channel> weakSelf = weak_from_this();
    loop_.queueInLoop([weakSelf, callId]
                      {
        if (const auto self = weakSelf.lock())
        {
            self->Dispatch(callId);
        } });
}

void Channel::CompleteCall(uint64_t callId, UnaryCallResult result)
{
    const auto it = calls_.find(callId);
    if (it == calls_.end())
    {
        return;
    }
    Call call = std::move(it->second);
    calls_.erase(it);
    deadlines_.erase({call.deadline, callId});
    if (call.active)
    {
        streamToCall_.erase({call.transportId, call.streamId});
    }
    // pending_ 里可能还留着这个 id:FlushPending / EnterTransientFailure 按 calls_ 查不到就跳过。
    loop_.queueInLoop([done = std::move(call.done), result = std::move(result)]() mutable
                      { done(std::move(result)); });
}

void Channel::FailSoon(UnaryCallback done, Status status)
{
    loop_.queueInLoop([done = std::move(done), status = std::move(status)]() mutable
                      { done(FailureResult(std::move(status))); });
}

void Channel::ExpireCall(uint64_t callId)
{
    const auto it = calls_.find(callId);
    if (it == calls_.end())
    {
        return;
    }
    const Call &call = it->second;
    std::shared_ptr<Http2Transport> transport;
    int32_t streamId = 0;
    std::chrono::steady_clock::time_point startedAt{};
    if (call.active)
    {
        transport = FindTransport(call.transportId);
        streamId = call.streamId;
        startedAt = call.startedAt;
    }
    const bool wasActive = call.active;
    // 先完成调用(恰好一次由 calls_ 保证),再取消流:取消触发的流结束事件会因找不到登记而被忽略。
    CompleteCall(callId, FailureResult(Status(StatusCode::kDeadlineExceeded,
                                              wasActive ? "deadline exceeded"
                                                        : "deadline exceeded while waiting for a connection")));
    if (transport)
    {
        transport->OnCallDeadlineExpired(startedAt);
        transport->ResetStream(streamId, kHttp2Cancel, Status(StatusCode::kDeadlineExceeded, "deadline exceeded"));
    }
}

void Channel::OnTransportReady(Http2Transport &transport)
{
    if (current_.get() != &transport)
    {
        return;
    }
    LOG_INFO << "muduo_grpc channel " << target_ << " is READY (connection #" << transport.id() << ")";
    nextBackoffMs_ = static_cast<double>(options_.backoff.initial.count());
    lastFailure_ = Status::Ok();
    SetState(ConnectivityState::kReady);
    // 在 nghttp2 的回调栈里:不同步提交新流,投递到本轮之后。
    QueueFlushPending();
}

void Channel::OnTransportGoaway(Http2Transport &transport, int32_t lastStreamId, uint32_t errorCode,
                                std::string_view debugData)
{
    constexpr uint32_t kHttp2NoError = 0x0;
    if (errorCode == kHttp2NoError)
    {
        LOG_INFO << "muduo_grpc channel " << target_ << " received GOAWAY on connection #" << transport.id()
                 << " last_stream_id=" << lastStreamId << " debug='" << std::string(debugData)
                 << "'; new calls move to a new connection";
    }
    else
    {
        LOG_WARN << "muduo_grpc channel " << target_ << " received GOAWAY on connection #" << transport.id()
                 << " error_code=" << errorCode << " last_stream_id=" << lastStreamId << " debug='"
                 << std::string(debugData) << "'";
    }
    if (current_.get() == &transport)
    {
        RetireCurrentTransport();
    }
}

void Channel::OnTransportStreamClosed(Http2Transport &transport, int32_t streamId, StreamOutcome outcome)
{
    const auto mapping = streamToCall_.find({transport.id(), streamId});
    if (mapping == streamToCall_.end())
    {
        MaybeReleaseDrainingTransport(transport); // 已提前完成(截止 / 析构)的调用
        return;
    }
    const uint64_t callId = mapping->second;
    streamToCall_.erase(mapping);
    const auto it = calls_.find(callId);
    if (it == calls_.end())
    {
        MaybeReleaseDrainingTransport(transport);
        return;
    }
    Call &call = it->second;
    call.active = false;
    call.streamId = 0;
    const auto now = std::chrono::steady_clock::now();

    if (outcome.unprocessed && !call.transparentRetried && now < call.deadline &&
        state_ != ConnectivityState::kShutdown)
    {
        // §4.6:对端可证明没处理 → 内部重发一次。投递到下一轮,不在 nghttp2 回调栈里提交新流。
        call.transparentRetried = true;
        QueueRedispatch(callId);
        MaybeReleaseDrainingTransport(transport);
        return;
    }
    if (outcome.status.code() == StatusCode::kCancelled && now >= call.deadline)
    {
        // 对端因 grpc-timeout 到期回 RST_STREAM(CANCEL):与 grpc-go 一样报 DEADLINE_EXCEEDED。
        outcome.status = Status(StatusCode::kDeadlineExceeded, "deadline exceeded (stream cancelled by peer)");
    }
    UnaryCallResult result;
    result.status = std::move(outcome.status);
    result.initialMetadata = std::move(outcome.initialMetadata);
    result.trailingMetadata = std::move(outcome.trailingMetadata);
    result.response = std::move(outcome.response);
    CompleteCall(callId, std::move(result));
    MaybeReleaseDrainingTransport(transport);
}

void Channel::OnTransportClosed(Http2Transport &transport, const Status &reason)
{
    if (current_.get() == &transport)
    {
        std::shared_ptr<Http2Transport> closed = std::move(current_);
        current_.reset();
        const ConnectivityState previous = state_;
        ReleaseTransport(std::move(closed));
        if (previous == ConnectivityState::kShutdown)
        {
            return;
        }
        if (previous == ConnectivityState::kReady)
        {
            // pick_first 语义:READY 连接断开回 IDLE,有排队调用就立即重连。
            SetState(ConnectivityState::kIdle);
            if (!pending_.empty())
            {
                StartConnecting();
            }
            return;
        }
        EnterTransientFailure(reason);
        return;
    }
    const auto it = std::find_if(draining_.begin(), draining_.end(),
                                 [&transport](const std::shared_ptr<Http2Transport> &candidate)
                                 { return candidate.get() == &transport; });
    if (it != draining_.end())
    {
        std::shared_ptr<Http2Transport> closed = std::move(*it);
        draining_.erase(it);
        ReleaseTransport(std::move(closed));
    }
}

void Channel::StartConnecting()
{
    if (current_ || state_ == ConnectivityState::kShutdown)
    {
        return;
    }
    SetState(ConnectivityState::kConnecting);
    CreateTransport();
}

void Channel::CreateTransport()
{
    const uint64_t transportId = nextTransportId_++;
    std::weak_ptr<Http2TransportListener> listener = std::static_pointer_cast<Http2TransportListener>(shared_from_this());
    current_ = Http2Transport::Create(loop_, address_, "muduo_grpc:" + target_ + "#" + std::to_string(transportId),
                                      transportOptions_, std::move(listener), transportId);
    current_->Connect();
}

void Channel::RetireCurrentTransport()
{
    std::shared_ptr<Http2Transport> retired = std::move(current_);
    current_.reset();
    if (state_ != ConnectivityState::kShutdown && state_ != ConnectivityState::kTransientFailure)
    {
        SetState(ConnectivityState::kIdle);
    }
    if (retired)
    {
        retired->MarkDraining();
        if (retired->ActiveStreamCount() == 0)
        {
            ReleaseTransport(std::move(retired));
        }
        else
        {
            draining_.push_back(std::move(retired));
        }
    }
    if (state_ == ConnectivityState::kTransientFailure)
    {
        // 后台重连的那条连接被退役了(例如握手前就收到 GOAWAY):重新排一次退避,否则通道会卡在 TF。
        ScheduleReconnect();
    }
    else if (state_ == ConnectivityState::kIdle && !pending_.empty())
    {
        StartConnecting();
    }
}

void Channel::EnterTransientFailure(const Status &reason)
{
    if (state_ != ConnectivityState::kTransientFailure)
    {
        LOG_WARN << "muduo_grpc channel " << target_ << " entered TRANSIENT_FAILURE: " << reason.ToString();
    }
    lastFailure_ = reason;
    SetState(ConnectivityState::kTransientFailure);
    std::deque<uint64_t> pending;
    pending.swap(pending_);
    for (const uint64_t callId : pending)
    {
        CompleteCall(callId, FailureResult(Status(StatusCode::kUnavailable,
                                                  "failed to connect to " + target_ + ": " + reason.message())));
    }
    ScheduleReconnect();
}

void Channel::ScheduleReconnect()
{
    if (backoffTimerArmed_)
    {
        return;
    }
    // gRPC connection-backoff:本次等待 = 当前退避 × (1 ± jitter),然后退避 ×multiplier,封顶 max。
    std::uniform_real_distribution<double> spread(-options_.backoff.jitter, options_.backoff.jitter);
    const double delayMs = std::max(0.0, nextBackoffMs_ * (1.0 + spread(rng_)));
    nextBackoffMs_ = std::min(nextBackoffMs_ * options_.backoff.multiplier,
                              static_cast<double>(options_.backoff.max.count()));
    std::weak_ptr<Channel> weakSelf = weak_from_this();
    backoffTimer_ = loop_.runAfter(delayMs / 1000.0, [weakSelf]
                                   {
        if (const auto self = weakSelf.lock())
        {
            self->OnBackoffTimer();
        } });
    backoffTimerArmed_ = true;
}

void Channel::OnBackoffTimer()
{
    backoffTimerArmed_ = false;
    if (state_ != ConnectivityState::kTransientFailure || current_)
    {
        return;
    }
    // 后台重连期间对外仍报 TRANSIENT_FAILURE(新调用立即失败),连上才转 READY。
    CreateTransport();
}

void Channel::ReleaseTransport(std::shared_ptr<Http2Transport> transport)
{
    if (!transport)
    {
        return;
    }
    transport->Shutdown();
    // 最后一个引用放到排队任务里释放:此刻可能正处在这条连接自己的回调栈上(§11.7)。
    loop_.queueInLoop([transport = std::move(transport)]() mutable { transport.reset(); });
}

void Channel::MaybeReleaseDrainingTransport(Http2Transport &transport)
{
    if (current_.get() == &transport || transport.ActiveStreamCount() != 0)
    {
        return;
    }
    const auto it = std::find_if(draining_.begin(), draining_.end(),
                                 [&transport](const std::shared_ptr<Http2Transport> &candidate)
                                 { return candidate.get() == &transport; });
    if (it == draining_.end())
    {
        return;
    }
    std::shared_ptr<Http2Transport> drained = std::move(*it);
    draining_.erase(it);
    ReleaseTransport(std::move(drained));
}

std::shared_ptr<Http2Transport> Channel::FindTransport(uint64_t transportId) const
{
    if (current_ && current_->id() == transportId)
    {
        return current_;
    }
    for (const auto &transport : draining_)
    {
        if (transport->id() == transportId)
        {
            return transport;
        }
    }
    return nullptr;
}

void Channel::ArmDeadlineTimer()
{
    if (deadlines_.empty())
    {
        return;
    }
    const auto earliest = deadlines_.begin()->first;
    if (deadlineTimerArmed_ && deadlineTimerFor_ <= earliest)
    {
        return;
    }
    if (deadlineTimerArmed_)
    {
        loop_.cancel(deadlineTimer_);
    }
    // muduo 定时器按墙钟走,只当唤醒提示;到点后用 steady_clock 复核,未到就重新 arm(§6)。
    // 加 1ms 余量,避免墙钟与单调钟的取整差让定时器早到一点而空转一轮。
    const double delaySeconds = ToSeconds(earliest - std::chrono::steady_clock::now()) + 0.001;
    std::weak_ptr<Channel> weakSelf = weak_from_this();
    deadlineTimer_ = loop_.runAfter(delaySeconds, [weakSelf]
                                    {
        if (const auto self = weakSelf.lock())
        {
            self->OnDeadlineTimer();
        } });
    deadlineTimerArmed_ = true;
    deadlineTimerFor_ = earliest;
}

void Channel::OnDeadlineTimer()
{
    deadlineTimerArmed_ = false;
    const auto now = std::chrono::steady_clock::now();
    std::vector<uint64_t> due;
    for (const auto &[deadline, callId] : deadlines_)
    {
        if (deadline > now)
        {
            break;
        }
        due.push_back(callId);
    }
    for (const uint64_t callId : due)
    {
        ExpireCall(callId);
    }
    ArmDeadlineTimer();
}

void Channel::SetState(ConnectivityState state)
{
    if (state_ == state)
    {
        return;
    }
    LOG_DEBUG << "muduo_grpc channel " << target_ << " state " << ConnectivityStateName(state_) << " -> "
              << ConnectivityStateName(state);
    state_ = state;
}

bool Channel::LoopIsCurrent() const
{
    return muduo::net::EventLoop::getEventLoopOfCurrentThread() == &loop_;
}

} // namespace muduo_grpc
