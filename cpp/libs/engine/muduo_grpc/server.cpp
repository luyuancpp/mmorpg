// 标准库头放在最前面:muduo 的 Windows 适配头 #define 了 off_t / pid_t 等名字(server.h 自己也是先标准库后 muduo)。
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "server.h"

#include "muduo/base/Logging.h"
#include "muduo/net/Acceptor.h"
#include "muduo/net/Buffer.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/SocketsOps.h"
#include "muduo/net/TcpConnection.h"

#include "address.h"

namespace muduo_grpc
{

namespace
{

using SteadyClock = std::chrono::steady_clock;
using Verdict = ServerSession::Verdict;

// muduo 一行日志的缓冲是 4000 字节,超长的片段会被整段丢弃:汇总行超过这个长度就截断后再打。
constexpr size_t kMaxSummaryLogBytes = 3500;

// 与 core 的 ShouldLogRpcEvent 同一规则(前 8 条逐条,之后每 1024 条一条);muduo_grpc 不依赖 core,在此本地实现。
bool ShouldLogServerEvent(uint64_t occurrence)
{
    return occurrence <= 8 || (occurrence & 1023) == 0;
}

double ToSeconds(SteadyClock::duration duration)
{
    return std::max(0.0, std::chrono::duration<double>(duration).count());
}

// 进程级:句柄在 loop 线程之外析构的次数(此时绝不能 lock Server)。
std::atomic<uint64_t> &DroppedOffLoopCounter()
{
    static std::atomic<uint64_t> counter{0};
    return counter;
}

// 截止 = start + timeout,饱和加法:99999999H 会被钳到 nanoseconds::max(),直接相加会溢出(§15.5.4)。
std::optional<SteadyClock::time_point> DeadlineFrom(SteadyClock::time_point start, std::chrono::nanoseconds timeout)
{
    const auto remaining = std::chrono::duration_cast<std::chrono::nanoseconds>(SteadyClock::time_point::max() - start);
    if (timeout >= remaining)
    {
        return std::nullopt; // 视为无截止
    }
    return start + std::chrono::duration_cast<SteadyClock::duration>(timeout);
}

const std::string &EmptyString()
{
    static const std::string kEmpty;
    return kEmpty;
}

const Metadata &EmptyMetadata()
{
    static const Metadata kEmpty;
    return kEmpty;
}

Verdict AcceptVerdict()
{
    return Verdict{Verdict::Kind::kAccept, Status()};
}

Verdict RejectVerdict(Status status)
{
    return Verdict{Verdict::Kind::kReject, std::move(status)};
}

Verdict RefuseVerdict()
{
    return Verdict{Verdict::Kind::kRefuse, Status()};
}

// ServerCall 除只读访问外只能在 loop 线程调用:与 muduo assertInLoopThread 同样 fail-fast。
void RequireLoopThread(const detail::CallToken &token, const char *what)
{
    if (std::this_thread::get_id() != token.loopThread)
    {
        LOG_FATAL << "muduo_grpc::ServerCall::" << what
                  << " called outside the server EventLoop thread, method=" << token.method;
    }
}

std::string FormatBudget(std::chrono::microseconds budget)
{
    if (budget.count() % 1000 == 0)
    {
        return std::to_string(budget.count() / 1000) + "ms";
    }
    return std::to_string(budget.count()) + "us";
}

} // namespace

// ── ServerCall / CallToken ─────────────────────────────────────────────

ServerCall::ServerCall(std::shared_ptr<detail::CallToken> token) : token_(std::move(token))
{
}

uint64_t ServerCall::id() const
{
    return token_ ? token_->callId : 0;
}

const std::string &ServerCall::method() const
{
    return token_ ? token_->method : EmptyString();
}

const std::string &ServerCall::peer() const
{
    return token_ ? token_->peer : EmptyString();
}

const Metadata &ServerCall::metadata() const
{
    return token_ ? token_->metadata : EmptyMetadata();
}

std::optional<std::chrono::steady_clock::time_point> ServerCall::deadline() const
{
    return token_ ? token_->deadline : std::nullopt;
}

bool ServerCall::IsActive() const
{
    if (!token_)
    {
        return false;
    }
    RequireLoopThread(*token_, "IsActive");
    const auto server = token_->server.lock();
    return server && server->calls_.count(token_->callId) != 0;
}

bool ServerCall::ServerDraining() const
{
    if (!token_)
    {
        return true;
    }
    RequireLoopThread(*token_, "ServerDraining");
    const auto server = token_->server.lock();
    return !server || server->draining();
}

bool ServerCall::TryStartBusiness() const
{
    if (!token_)
    {
        return false;
    }
    RequireLoopThread(*token_, "TryStartBusiness");
    const auto server = token_->server.lock();
    return server && server->TryStartBusiness(token_->callId);
}

void ServerCall::FinishOk(std::string responseBytes) const
{
    if (!token_)
    {
        LOG_ERROR << "muduo_grpc::ServerCall::FinishOk called on an empty call handle";
        return;
    }
    RequireLoopThread(*token_, "FinishOk");
    const auto server = token_->server.lock();
    if (!server)
    {
        return; // 服务端已销毁:~Server 已静默收口全部调用
    }
    Server::CallEnd end;
    end.kind = Server::CallEndKind::kResponse;
    end.response = std::move(responseBytes);
    server->FinishFromHandler(*token_, std::move(end));
}

void ServerCall::FinishError(Status status) const
{
    if (!token_)
    {
        LOG_ERROR << "muduo_grpc::ServerCall::FinishError called on an empty call handle";
        return;
    }
    RequireLoopThread(*token_, "FinishError");
    if (status.ok())
    {
        LOG_ERROR << "muduo_grpc::ServerCall::FinishError called with an OK status, method=" << token_->method
                  << "; replying INTERNAL";
        status = Status(StatusCode::kInternal, "server finished the call with an OK status but no response");
    }
    const auto server = token_->server.lock();
    if (!server)
    {
        return;
    }
    Server::CallEnd end;
    end.kind = Server::CallEndKind::kStatus;
    end.status = std::move(status);
    server->FinishFromHandler(*token_, std::move(end));
}

detail::CallToken::~CallToken()
{
    if (std::this_thread::get_id() != loopThread)
    {
        // 不在 loop 线程:只计数,绝不 lock Server(lock 出来的临时强引用若是最后一个,Server 会在这个线程析构)。
        // 此时无从判断调用是否已完成,所以每次 loop 外析构都计一次;正常路径上恒为 0。
        DroppedOffLoopCounter().fetch_add(1, std::memory_order_relaxed);
        return;
    }
    if (const auto owner = server.lock())
    {
        owner->OnTokenDropped(*this);
    }
}

// ── detail::ServerConnection ──────────────────────────────────────────

namespace detail
{

// 每条连接一个:ServerSession 的宿主与 Observer,把字节在 muduo 连接与会话之间搬运。
// 只存 weak<TcpConnection>、weak<Server>(AGENTS §11.7);强持有者只有 Server::connections_ 与 connectDestroyed 闭包。
// 连接自己的定时器(握手 / GOAWAY-PING / 关闭宽限)只绑 weak<ServerConnection>。
// 不在它自己的回调栈上析构:摘表时 Server 把它移进 connectDestroyed 闭包一起释放。
class ServerConnection final : public std::enable_shared_from_this<ServerConnection>, private ServerSession::Observer
{
public:
    ServerConnection(muduo::net::EventLoop &loop, std::weak_ptr<Server> server, uint64_t id, std::string peer,
                     const ServerOptions &options)
        : loop_(loop),
          server_(std::move(server)),
          id_(id),
          peer_(std::move(peer)),
          maxPendingOutputBytes_(options.maxPendingOutputBytes),
          handshakeTimeout_(options.handshakeTimeout),
          maxTrackedHeaderTimes_(std::max<size_t>(16, static_cast<size_t>(options.session.maxConcurrentStreams) * 2)),
          session_(options.session, static_cast<ServerSession::Observer &>(*this))
    {
    }

    ~ServerConnection() override { CancelTimers(); }

    ServerConnection(const ServerConnection &) = delete;
    ServerConnection &operator=(const ServerConnection &) = delete;

    uint64_t id() const { return id_; }
    const std::string &peer() const { return peer_; }
    bool receivedAnyBytes() const { return session_.receivedAnyBytes(); }

    // connectEstablished 之后调用:建会话、上弦握手超时。本端 SETTINGS 只提交进 nghttp2,
    // 收到对端首字节之前不写(§15.5.1)。
    void Start(const muduo::net::TcpConnectionPtr &conn)
    {
        tcp_ = conn;
        const Status started = session_.Start();
        if (!started.ok())
        {
            LOG_ERROR << "[GrpcServer] failed to start the HTTP/2 session for " << peer_ << ": " << started.ToString();
            ForceClose();
            return;
        }
        if (handshakeTimeout_.count() > 0)
        {
            handshakeDeadline_ = SteadyClock::now() + handshakeTimeout_;
            ArmHandshakeTimer();
        }
    }

    void HandleMessage(muduo::net::Buffer &buffer)
    {
        if (closed_)
        {
            buffer.retrieveAll();
            return;
        }
        // muduo 一次读事件只做一次 readv;nghttp2 同步解完,这一步只做分帧和 HPACK(§15.6)。
        ++sessionDepth_;
        const Status status =
            session_.Receive(reinterpret_cast<const uint8_t *>(buffer.peek()), buffer.readableBytes());
        --sessionDepth_;
        buffer.retrieveAll();
        if (closed_)
        {
            return;
        }
        if (!status.ok())
        {
            // 坏 magic / 洪泛 / nghttp2 内部失败:会话不再写任何字节,直接关(§15.5.1)。
            ReportProtocolError(status);
            ForceClose();
            return;
        }
        ApplyDeferredResponses();
        if (finalGoawayRequested_)
        {
            // PING ACK 在 Receive 的回调栈里到达;最终 GOAWAY 推迟到这里提交。
            finalGoawayRequested_ = false;
            SubmitFinalGoaway();
        }
        FlushOutput(); // 首次:本端 SETTINGS;之后:SETTINGS ACK / PING ACK / WINDOW_UPDATE / 早拒
    }

    // 照抄 Http2Transport::FlushOutput 的 sessionDepth_ / flushDeferred_:不在会话回调栈里重入 nghttp2;
    // 输出缓冲达到上限就暂停取帧,writeComplete 后继续。写半关之前的最终冲出走 FlushFinalFrames,不受上限限制。
    void FlushOutput()
    {
        for (;;)
        {
            if (closed_ || !session_.receivedAnyBytes())
            {
                return; // 收到对端首字节之前不写(会话层也有同样的检查)
            }
            if (sessionDepth_ > 0)
            {
                flushDeferred_ = true;
                return;
            }
            const auto conn = tcp_.lock();
            if (!conn || !conn->connected())
            {
                return; // 已写半关或已断开:不再写(写半关之前 CloseGracefully 已把已提交的帧全部冲出)
            }
            if (conn->outputBuffer()->readableBytes() >= maxPendingOutputBytes_)
            {
                return;
            }
            flushDeferred_ = false;
            const Status status = DrainSessionInto(*conn, true);
            if (!status.ok())
            {
                // nghttp2 因对端违约发出了带错误码的 GOAWAY(字节已交给 send),或 mem_send 失败:
                // 冲出剩余的帧再写半关,宽限到期 forceClose(§15.5.6「尽力冲出后 forceClose」)。
                ReportProtocolError(status);
                CloseGracefully();
                return;
            }
            if (!deferred_.empty())
            {
                ApplyDeferredResponses();
                flushDeferred_ = true;
            }
            if (!flushDeferred_)
            {
                break;
            }
        }
        // 最终 GOAWAY 已写出且没有打开的流、或对端 GOAWAY 后没有流、或会话被终止:写半关,宽限后 forceClose。
        if (!closing_ && session_.ShouldClose())
        {
            CloseGracefully();
        }
    }

    // CompleteCall 的应答出口。对端已走(流已关闭 / 已应答)时 Submit* 返回 false,忽略即可。
    void Respond(int32_t streamId, Server::CallEnd end)
    {
        if (closed_ || end.kind == Server::CallEndKind::kSilent)
        {
            return;
        }
        if (sessionDepth_ > 0)
        {
            // 不在 nghttp2 回调栈里提交(ServerSession 的约定);回调返回后由 ApplyDeferredResponses 补交。
            deferred_.push_back(DeferredResponse{streamId, std::move(end)});
            flushDeferred_ = true;
            return;
        }
        SubmitNow(streamId, std::move(end));
    }

    void BindStream(int32_t streamId, uint64_t callId) { streamCalls_[streamId] = callId; }
    void ForgetStream(int32_t streamId) { streamCalls_.erase(streamId); }

    std::optional<uint64_t> TakeStream(int32_t streamId)
    {
        const auto it = streamCalls_.find(streamId);
        if (it == streamCalls_.end())
        {
            return std::nullopt;
        }
        const uint64_t callId = it->second;
        streamCalls_.erase(it);
        return callId;
    }

    std::vector<uint64_t> TakeAllCalls()
    {
        std::vector<uint64_t> callIds;
        callIds.reserve(streamCalls_.size());
        for (const auto &entry : streamCalls_)
        {
            callIds.push_back(entry.second);
        }
        streamCalls_.clear();
        std::sort(callIds.begin(), callIds.end());
        return callIds;
    }

    // 两段式 GOAWAY 第一段(§15.5.6):GOAWAY(2^31-1) + PING;第二段在 PING ACK 或满 goawayPingTimeout 时发。
    // 冲出由调用方负责(Server::BeginShutdown 标记 dirty)。
    void BeginShutdown(std::chrono::milliseconds goawayPingTimeout, std::chrono::milliseconds closeGrace)
    {
        if (closed_ || shutdownStarted_)
        {
            return;
        }
        shutdownStarted_ = true;
        closeGrace_ = closeGrace;
        if (closing_)
        {
            return; // 已在写半关(协议错误 / 对端 GOAWAY):不再发 GOAWAY
        }
        session_.SubmitShutdownNotice();
        pingDeadline_ = SteadyClock::now() + goawayPingTimeout;
        ArmPingTimer();
    }

    // 排空到期(§15.10):补最终 GOAWAY,然后优雅关闭 —— CloseGracefully 先把刚提交的 REFUSED / UNAVAILABLE
    // 与最终 GOAWAY 全部交给输出缓冲(输出被背压时也一样),再写半关;宽限到期 forceClose。
    // 已在关闭(此前已写半关或已 forceClose)时 CloseGracefully 不再冲出:写半关之后 TcpConnection::send 本来就丢弃字节。
    void ExpireDrain()
    {
        if (closed_)
        {
            return;
        }
        if (!session_.receivedAnyBytes())
        {
            ForceClose();
            return;
        }
        SubmitFinalGoaway();
        CloseGracefully();
    }

    void ForceClose()
    {
        closing_ = true;
        if (const auto conn = tcp_.lock())
        {
            conn->forceClose();
        }
    }

    // 摘表时调用:取消定时器,之后不再读写、不再回调 Server。
    void Close()
    {
        closed_ = true;
        closing_ = true;
        CancelTimers();
        deferred_.clear();
        headersReceivedAt_.clear();
    }

private:
    struct DeferredResponse
    {
        int32_t streamId{0};
        Server::CallEnd end;
    };

    // ---- ServerSession::Observer:全部在 Receive / DrainOutput 的 nghttp2 回调栈里,只查表、登记、入队 ----

    void OnClientSettings() override
    {
        if (handshakeTimerArmed_)
        {
            loop_.cancel(handshakeTimer_);
            handshakeTimerArmed_ = false;
        }
    }

    Verdict OnRequestHeaders(int32_t streamId, const RequestHeaders &headers) override
    {
        const auto server = server_.lock();
        if (!server)
        {
            return RefuseVerdict();
        }
        Verdict verdict = server->RouteRequest(headers);
        if (verdict.kind == Verdict::Kind::kAccept)
        {
            RememberHeadersTime(streamId);
        }
        return verdict;
    }

    Verdict OnRequestComplete(int32_t streamId, RequestHeaders headers, std::string message) override
    {
        // 截止从头部收完时起算(§15.5.4);记录被修剪掉(见 RememberHeadersTime)时退回「现在」。
        SteadyClock::time_point headersAt = SteadyClock::now();
        if (const auto it = headersReceivedAt_.find(streamId); it != headersReceivedAt_.end())
        {
            headersAt = it->second;
            headersReceivedAt_.erase(it);
        }
        const auto server = server_.lock();
        if (!server)
        {
            return RefuseVerdict();
        }
        return server->AcceptCall(*this, streamId, std::move(headers), std::move(message), headersAt);
    }

    void OnStreamAborted(int32_t streamId, uint32_t /*http2ErrorCode*/) override
    {
        headersReceivedAt_.erase(streamId);
        if (const auto server = server_.lock())
        {
            server->AbortStream(*this, streamId);
        }
    }

    void OnShutdownPingAck() override
    {
        // 在 Receive 的回调栈里:不得同步提交,HandleMessage 在 Receive 返回后提交最终 GOAWAY。
        finalGoawayRequested_ = true;
        if (pingTimerArmed_)
        {
            loop_.cancel(pingTimer_);
            pingTimerArmed_ = false;
        }
    }

    void OnRequestRejected(int32_t streamId, int httpStatus, const Status &status) override
    {
        headersReceivedAt_.erase(streamId);
        if (const auto server = server_.lock())
        {
            server->OnSessionRejected(*this, httpStatus, status);
        }
    }

    // 头部收完时刻按流登记,收完请求时取走。头部之后被对端 RST 或被 nghttp2 关掉的流不会再有回调,
    // 记录会残留:超过上限就丢掉最老的一半(流 id 单调递增;被丢的流若仍在上传,截止退回从收完时起算)。
    void RememberHeadersTime(int32_t streamId)
    {
        headersReceivedAt_[streamId] = SteadyClock::now();
        if (headersReceivedAt_.size() <= maxTrackedHeaderTimes_)
        {
            return;
        }
        while (headersReceivedAt_.size() > maxTrackedHeaderTimes_ / 2)
        {
            headersReceivedAt_.erase(headersReceivedAt_.begin());
        }
    }

    // 每条连接只计一次 protocol_errors(Receive 的致命错误是粘滞的,forceClose 之前可能再报)。
    void ReportProtocolError(const Status &status)
    {
        if (protocolErrorReported_)
        {
            return;
        }
        protocolErrorReported_ = true;
        if (const auto server = server_.lock())
        {
            server->OnConnectionProtocolError(*this, status);
        }
    }

    void SubmitNow(int32_t streamId, Server::CallEnd end)
    {
        switch (end.kind)
        {
        case Server::CallEndKind::kResponse:
            session_.SubmitResponse(streamId, std::move(end.response));
            return;
        case Server::CallEndKind::kStatus:
            session_.SubmitStatus(streamId, end.status);
            return;
        case Server::CallEndKind::kRefuse:
            session_.RefuseStream(streamId);
            return;
        case Server::CallEndKind::kSilent:
            return;
        }
    }

    void ApplyDeferredResponses()
    {
        if (deferred_.empty() || sessionDepth_ > 0)
        {
            return;
        }
        std::vector<DeferredResponse> pending;
        pending.swap(deferred_);
        for (auto &response : pending)
        {
            SubmitNow(response.streamId, std::move(response.end));
        }
    }

    void SubmitFinalGoaway()
    {
        if (sessionDepth_ > 0)
        {
            finalGoawayRequested_ = true;
            return;
        }
        if (pingTimerArmed_)
        {
            loop_.cancel(pingTimer_);
            pingTimerArmed_ = false;
        }
        session_.SubmitFinalGoaway();
    }

    // 把 nghttp2 里排队的帧交给 muduo。respectBackpressure:输出缓冲达到 maxPendingOutputBytes_ 就先停
    // (writeComplete 后由 FlushOutput 继续);为假时一次取完,只用于写半关之前的最终冲出(FlushFinalFrames)。
    // 调用方保证不在会话回调栈里(sessionDepth_ == 0)。
    Status DrainSessionInto(muduo::net::TcpConnection &conn, bool respectBackpressure)
    {
        ++sessionDepth_;
        const Status status = session_.DrainOutput([this, &conn, respectBackpressure](std::string_view chunk)
                                                   {
            conn.send(chunk.data(), static_cast<int>(chunk.size()));
            return !respectBackpressure || conn.outputBuffer()->readableBytes() < maxPendingOutputBytes_; });
        --sessionDepth_;
        return status;
    }

    // 写半关之前的最终冲出(§15.16 第 2 条):把 nghttp2 里已提交的帧全部交给输出缓冲,这一次不看
    // maxPendingOutputBytes_。写半关之后连接不再是 kConnected,TcpConnection::send 直接丢弃字节,
    // 此刻还留在 nghttp2 里的 REFUSED_STREAM / UNAVAILABLE / 最终 GOAWAY 就再也写不出去,对端只看到 EOF。
    // 量有界:控制帧(若干 RST_STREAM / trailers-only / GOAWAY)加上对端流控窗口允许的、已提交应答的 DATA ——
    // 后者本来就在内存里(只是从 nghttp2 挪进输出缓冲),写不出去时由宽限到期的 forceClose 一并释放。
    void FlushFinalFrames(muduo::net::TcpConnection &conn)
    {
        if (sessionDepth_ > 0 || !conn.connected() || !session_.receivedAnyBytes())
        {
            // 调用方(CloseGracefully)都在会话回调栈之外、写半关之前,前两条不会成立;
            // 收到对端首字节之前本来就一个字节都不写(§15.5.1)。
            return;
        }
        do
        {
            ApplyDeferredResponses();
            const Status status = DrainSessionInto(conn, false);
            if (!status.ok())
            {
                // 已在关闭:只计数,不再升级处理;已交出的字节照常随 FIN 发出。
                ReportProtocolError(status);
            }
        } while (!deferred_.empty());
        flushDeferred_ = false;
    }

    // 优雅关闭 = 先把已提交的帧全部写进输出缓冲(FlushFinalFrames),再写半关 —— muduo 在输出缓冲写空之后才真正
    // shutdownWrite(发 FIN);closeGrace 是硬上限,到期 forceClose(对端不读、输出缓冲写不空时由它兜底)。
    // 只在会话回调栈之外调用(FlushOutput 在栈内直接返回;ExpireDrain 来自排空定时器)。
    void CloseGracefully()
    {
        if (closing_ || closed_)
        {
            return;
        }
        closing_ = true;
        const auto conn = tcp_.lock();
        if (!conn)
        {
            return;
        }
        FlushFinalFrames(*conn);
        conn->shutdown();
        graceDeadline_ = SteadyClock::now() + closeGrace_;
        ArmGraceTimer();
    }

    void ArmHandshakeTimer()
    {
        const std::weak_ptr<ServerConnection> weakSelf = weak_from_this();
        handshakeTimer_ = loop_.runAfter(ToSeconds(handshakeDeadline_ - SteadyClock::now()) + 0.001, [weakSelf]
                                         {
            if (const auto self = weakSelf.lock())
            {
                self->OnHandshakeTimer();
            } });
        handshakeTimerArmed_ = true;
    }

    void OnHandshakeTimer()
    {
        handshakeTimerArmed_ = false;
        if (closed_ || session_.handshakeComplete())
        {
            return;
        }
        if (SteadyClock::now() < handshakeDeadline_)
        {
            ArmHandshakeTimer(); // 墙钟前跳让定时器早到:按单调钟剩余时间重新上弦
            return;
        }
        if (const auto server = server_.lock())
        {
            server->OnHandshakeTimeout(*this);
        }
        ForceClose();
    }

    void ArmPingTimer()
    {
        const std::weak_ptr<ServerConnection> weakSelf = weak_from_this();
        pingTimer_ = loop_.runAfter(ToSeconds(pingDeadline_ - SteadyClock::now()) + 0.001, [weakSelf]
                                    {
            if (const auto self = weakSelf.lock())
            {
                self->OnPingTimer();
            } });
        pingTimerArmed_ = true;
    }

    void OnPingTimer()
    {
        pingTimerArmed_ = false;
        if (closed_)
        {
            return;
        }
        if (SteadyClock::now() < pingDeadline_)
        {
            ArmPingTimer();
            return;
        }
        // 满 goawayPingTimeout 仍没收到 PING ACK(对端慢,或 nghttp2 因已有 GOAWAY 而没发 notice / PING):直接发第二段。
        SubmitFinalGoaway();
        FlushOutput();
    }

    void ArmGraceTimer()
    {
        const std::weak_ptr<ServerConnection> weakSelf = weak_from_this();
        graceTimer_ = loop_.runAfter(ToSeconds(graceDeadline_ - SteadyClock::now()) + 0.001, [weakSelf]
                                     {
            if (const auto self = weakSelf.lock())
            {
                self->OnGraceTimer();
            } });
        graceTimerArmed_ = true;
    }

    void OnGraceTimer()
    {
        graceTimerArmed_ = false;
        if (closed_)
        {
            return;
        }
        if (SteadyClock::now() < graceDeadline_)
        {
            ArmGraceTimer();
            return;
        }
        ForceClose();
    }

    void CancelTimers()
    {
        // 进程退出路径上 EventLoop 可能已不在本线程(或已析构):定时器随 loop 一起释放,闭包只持 weak_ptr。
        if (muduo::net::EventLoop::getEventLoopOfCurrentThread() != &loop_)
        {
            handshakeTimerArmed_ = false;
            pingTimerArmed_ = false;
            graceTimerArmed_ = false;
            return;
        }
        if (handshakeTimerArmed_)
        {
            loop_.cancel(handshakeTimer_);
            handshakeTimerArmed_ = false;
        }
        if (pingTimerArmed_)
        {
            loop_.cancel(pingTimer_);
            pingTimerArmed_ = false;
        }
        if (graceTimerArmed_)
        {
            loop_.cancel(graceTimer_);
            graceTimerArmed_ = false;
        }
    }

    muduo::net::EventLoop &loop_;
    const std::weak_ptr<Server> server_;
    const uint64_t id_;
    const std::string peer_;
    const size_t maxPendingOutputBytes_;
    const std::chrono::milliseconds handshakeTimeout_;
    const size_t maxTrackedHeaderTimes_;
    std::weak_ptr<muduo::net::TcpConnection> tcp_;   // §11.7:只存 weak,强持有者是 Server::connections_
    ServerSession session_;

    std::unordered_map<int32_t, uint64_t> streamCalls_;                  // streamId → callId(已登记的调用)
    std::map<int32_t, SteadyClock::time_point> headersReceivedAt_;      // 按流 id 有序,便于修剪最老的
    std::vector<DeferredResponse> deferred_;

    // >0 表示正处在 session_ 的 Receive / DrainOutput 调用栈里:此时不重入 nghttp2。
    int sessionDepth_{0};
    bool flushDeferred_{false};
    bool closed_{false};                 // 已摘表
    bool closing_{false};                // 已写半关或已 forceClose
    bool shutdownStarted_{false};        // 已发第一段 GOAWAY
    bool finalGoawayRequested_{false};   // PING ACK 已到,等 Receive 返回后提交第二段
    bool protocolErrorReported_{false};

    std::chrono::milliseconds closeGrace_{ServerShutdownOptions{}.closeGrace};

    // muduo 定时器按墙钟走,只当唤醒提示;到点按 steady_clock 复核。
    muduo::net::TimerId handshakeTimer_;
    bool handshakeTimerArmed_{false};
    SteadyClock::time_point handshakeDeadline_{};
    muduo::net::TimerId pingTimer_;
    bool pingTimerArmed_{false};
    SteadyClock::time_point pingDeadline_{};
    muduo::net::TimerId graceTimer_;
    bool graceTimerArmed_{false};
    SteadyClock::time_point graceDeadline_{};
};

} // namespace detail

// ── Server ────────────────────────────────────────────────────────────

class Server::BatchScope
{
public:
    explicit BatchScope(Server &server) : server_(server) { ++server_.batchDepth_; }
    ~BatchScope()
    {
        if (--server_.batchDepth_ == 0)
        {
            server_.FlushDirtyConnections();
        }
    }
    BatchScope(const BatchScope &) = delete;
    BatchScope &operator=(const BatchScope &) = delete;

private:
    Server &server_;
};

std::shared_ptr<Server> Server::Create(muduo::net::EventLoop &loop, const std::string &ipv4, uint16_t port,
                                       ServerOptions options)
{
    // muduo InetAddress 遇到非法 IP 会静默落成 0.0.0.0(监听全部网卡),必须先校验(§15.3 Q8)。
    if (!IsCanonicalIpv4(ipv4) || port == 0)
    {
        LOG_ERROR << "muduo_grpc::Server rejects listen address '" << ipv4 << ":" << port
                  << "': only canonical IPv4 literals with a non-zero port are supported";
        return nullptr;
    }
    return std::make_shared<Server>(PrivateTag{}, loop, muduo::net::InetAddress(ipv4, port), std::move(options));
}

Server::Server(PrivateTag, muduo::net::EventLoop &loop, const muduo::net::InetAddress &listenAddress,
               ServerOptions options)
    : loop_(loop),
      listenInetAddress_(listenAddress),
      listenAddress_(listenAddress.toIpPort()),
      options_(std::move(options))
{
}

Server::~Server()
{
    // 照 TcpServer::~TcpServer:只在 loop 线程析构。
    loop_.assertInLoopThread();
    CancelTimers();
    // 剩余调用静默收口:之后的 Finish / 句柄析构经 weak_ptr 落空。
    calls_.clear();
    ready_.clear();
    deadlines_.clear();
    dirtyConnections_.clear();
    // 剩余连接逐条 forceClose:forceCloseInLoop 排进队列并持有连接;随后 handleClose 调 close 回调,
    // 回调里 lock 不到 Server,照样无条件排 connectDestroyed(两趟都由节点的两跳 quit 覆盖)。
    for (auto &entry : connections_)
    {
        entry.second.connection->Close();
        entry.second.tcp->forceClose();
    }
    connections_.clear();
    // 没 BeginShutdown 就析构时 Acceptor 在这里同步销毁(见头文件「析构前置」)。
    acceptor_.reset();
    onDrained_ = nullptr; // 不调 onDrained
}

bool Server::RegisterService(Service &service)
{
    loop_.assertInLoopThread();
    if (phase_ != Phase::kCreated)
    {
        LOG_ERROR << "muduo_grpc::Server " << listenAddress_ << ": RegisterService must be called before Start";
        return false;
    }
    const bool validBefore = methodTable_.valid();
    service.RegisterMethods(methodTable_);
    return validBefore && methodTable_.valid();
}

void Server::Start()
{
    loop_.assertInLoopThread();
    if (phase_ != Phase::kCreated)
    {
        LOG_ERROR << "muduo_grpc::Server " << listenAddress_ << ": Start ignored (already started or shutting down)";
        return;
    }
    if (!methodTable_.valid())
    {
        LOG_FATAL << "muduo_grpc::Server " << listenAddress_
                  << " refuses to start: the method table has malformed or duplicate entries (see earlier errors)";
    }
    if (weak_from_this().expired())
    {
        LOG_FATAL << "muduo_grpc::Server must be owned by shared_ptr (use Server::Create) before Start()";
    }

    // 冻结方法表:下标即 methodIndex(派发与按方法统计用);统计名取方法名,同名时退回完整路径。
    const std::vector<std::string> paths = methodTable_.Paths();
    std::map<std::string, size_t, std::less<>> methodNameCount;
    std::set<std::string, std::less<>> services;
    for (const auto &path : paths)
    {
        const auto parts = SplitMethodPath(path);
        if (parts)
        {
            ++methodNameCount[std::string(parts->second)];
            services.emplace(parts->first);
        }
    }
    methods_.reserve(paths.size());
    for (const auto &path : paths)
    {
        const auto parts = SplitMethodPath(path);
        const UnaryMethodHandler *handler = methodTable_.Find(path);
        if (!parts || handler == nullptr)
        {
            continue; // 不可达:AddUnary 已校验
        }
        MethodEntry entry;
        entry.path = path;
        const std::string methodName(parts->second);
        entry.displayName = methodNameCount[methodName] > 1 ? path : methodName;
        entry.handler = *handler;
        methodIndex_.emplace(path, methods_.size());
        methods_.push_back(std::move(entry));
    }
    stats_.methodCodes.assign(methods_.size(), {});

    phase_ = Phase::kRunning;
    const std::weak_ptr<Server> weakSelf = weak_from_this();
    acceptor_ = std::make_shared<muduo::net::Acceptor>(&loop_, listenInetAddress_, false);
    acceptor_->setNewConnectionCallback([weakSelf](int sockfd, const muduo::net::InetAddress &peerAddress)
                                        {
        if (const auto self = weakSelf.lock())
        {
            self->HandleNewConnection(sockfd, peerAddress);
            return;
        }
        muduo::net::sockets::close(sockfd); });
    acceptor_->listen();

    if (options_.statsLogInterval.count() > 0)
    {
        statsTimer_ = loop_.runEvery(static_cast<double>(options_.statsLogInterval.count()), [weakSelf]
                                     {
            if (const auto self = weakSelf.lock())
            {
                self->LogStatsIfChanged();
            } });
        statsTimerArmed_ = true;
    }

    std::string serviceList;
    for (const auto &service : services)
    {
        if (!serviceList.empty())
        {
            serviceList += ',';
        }
        serviceList += service;
    }
    LOG_INFO << "gRPC server (muduo_grpc) listening on " << listenAddress_ << " services=[" << serviceList
             << "] methods=" << methods_.size() << " max_streams_per_conn=" << options_.session.maxConcurrentStreams
             << " max_in_flight=" << options_.maxInFlightCalls << " dispatch_per_turn=" << options_.maxDispatchPerTurn
             << "/" << FormatBudget(options_.dispatchTurnBudget) << " threads=0";
}

bool Server::draining() const
{
    loop_.assertInLoopThread();
    return phase_ == Phase::kDraining || phase_ == Phase::kDrainExpired || phase_ == Phase::kDrained;
}

size_t Server::inFlightCalls() const
{
    loop_.assertInLoopThread();
    return calls_.size();
}

size_t Server::connectionCount() const
{
    loop_.assertInLoopThread();
    return connections_.size();
}

void Server::HandleNewConnection(int sockfd, const muduo::net::InetAddress &peerAddress)
{
    loop_.assertInLoopThread();
    if (phase_ != Phase::kRunning)
    {
        // 排空开始之后、Acceptor 被排队销毁之前到达的接入(§15.10 b):直接关掉,不建连接。
        muduo::net::sockets::close(sockfd);
        return;
    }
    // 以下照抄 TcpServer::newConnection 的时序:建连接 → 入表 → 设回调 → connectEstablished。
    const uint64_t connectionId = nextConnectionId_++;
    ++stats_.accepted;
    const std::string name = options_.name + "-" + listenAddress_ + "#" + std::to_string(connectionId);
    const muduo::net::InetAddress localAddress(muduo::net::sockets::getLocalAddr(sockfd));
    const auto conn = std::make_shared<muduo::net::TcpConnection>(&loop_, name, sockfd, localAddress, peerAddress);
    const auto connection = std::make_shared<detail::ServerConnection>(loop_, weak_from_this(), connectionId,
                                                                       peerAddress.toIpPort(), options_);
    connections_.emplace(connectionId, ConnectionEntry{conn, connection});
    stats_.connectionsPeak = std::max<uint64_t>(stats_.connectionsPeak, connections_.size());

    // connectEstablished / handleClose / connectDestroyed 无条件调用 connectionCallback_:空的 std::function 会抛异常,
    // 必须设。连接的建立与断开由本类自己记录,这里给空函数(Linux 上游 TcpServer 每次建连 / 断连各打一行 INFO)。
    conn->setConnectionCallback([](const muduo::net::TcpConnectionPtr &) {});
    const std::weak_ptr<detail::ServerConnection> weakConnection = connection;
    conn->setMessageCallback(
        [weakConnection](const muduo::net::TcpConnectionPtr &, muduo::net::Buffer *buffer, muduo::Timestamp)
        {
            if (const auto self = weakConnection.lock())
            {
                self->HandleMessage(*buffer);
                return;
            }
            buffer->retrieveAll();
        });
    conn->setWriteCompleteCallback([weakConnection](const muduo::net::TcpConnectionPtr &)
                                   {
        if (const auto self = weakConnection.lock())
        {
            self->FlushOutput();
        } });
    // close 回调绑 weak<Server>(上游绑裸 this 并标了 FIXME)。照抄 removeConnectionInLoop:摘表后无条件排 connectDestroyed;
    // 摘下的 ServerConnection 移进同一个闭包,不在它自己的回调栈上释放。排空完成检查排在 connectDestroyed 之后(FIFO)。
    const std::weak_ptr<Server> weakSelf = weak_from_this();
    conn->setCloseCallback([weakSelf, connectionId](const muduo::net::TcpConnectionPtr &closed)
                           {
        std::shared_ptr<detail::ServerConnection> detached;
        const auto self = weakSelf.lock();
        if (self)
        {
            detached = self->DetachConnection(connectionId);
        }
        closed->getLoop()->queueInLoop([closed, detached] { closed->connectDestroyed(); });
        if (self)
        {
            self->MaybeFinishDrain();
        } });
    conn->setTcpNoDelay(true);
    conn->connectEstablished();
    connection->Start(conn);
}

std::shared_ptr<detail::ServerConnection> Server::DetachConnection(uint64_t connectionId)
{
    const auto it = connections_.find(connectionId);
    if (it == connections_.end())
    {
        return nullptr;
    }
    std::shared_ptr<detail::ServerConnection> connection = std::move(it->second.connection);
    connections_.erase(it);
    dirtyConnections_.erase(connectionId);
    connection->Close();
    // §15.7 第 5 条:逐个撤销这条连接上的调用,所以连接表为空就意味着调用表为空。
    for (const uint64_t callId : connection->TakeAllCalls())
    {
        const auto phase = CompleteCall(callId, CallEnd{});
        if (!phase)
        {
            continue;
        }
        if (*phase == CallPhase::kStarted)
        {
            ++stats_.abortedAfterStart;
        }
        else
        {
            ++stats_.cancelledBeforeStart;
        }
    }
    if (!connection->receivedAnyBytes())
    {
        // 探针(kubelet tcpSocket)连上就关:本端一个字节都没写,只打 DEBUG(§15.5.1)。
        ++stats_.closedBeforePreface;
        LOG_DEBUG << "[GrpcServer] connection from " << connection->peer() << " closed before the HTTP/2 preface";
    }
    else
    {
        LOG_DEBUG << "[GrpcServer] connection from " << connection->peer() << " closed";
    }
    return connection;
}

void Server::MarkConnectionDirty(uint64_t connectionId)
{
    if (batchDepth_ > 0)
    {
        dirtyConnections_.insert(connectionId);
        return;
    }
    const auto it = connections_.find(connectionId);
    if (it != connections_.end())
    {
        const auto connection = it->second.connection; // 冲出期间保活
        connection->FlushOutput();
    }
}

void Server::FlushDirtyConnections()
{
    while (!dirtyConnections_.empty())
    {
        std::set<uint64_t> dirty;
        dirty.swap(dirtyConnections_);
        for (const uint64_t connectionId : dirty)
        {
            const auto it = connections_.find(connectionId);
            if (it != connections_.end())
            {
                const auto connection = it->second.connection;
                connection->FlushOutput();
            }
        }
    }
}

ServerSession::Verdict Server::RouteRequest(const RequestHeaders &headers)
{
    if (phase_ == Phase::kDrainExpired || phase_ == Phase::kDrained)
    {
        ++stats_.refusedOnShutdown; // 排空到期之后才开的流:没处理,客户端可透明重试(§15.3 Q4)
        return RefuseVerdict();
    }
    if (methodIndex_.find(std::string_view(headers.path)) != methodIndex_.end())
    {
        return AcceptVerdict();
    }
    // ServerSession 保证 path 能拆开;拆不开时按 grpc-go 同文案兜底。
    Status status;
    const auto parts = SplitMethodPath(headers.path);
    if (!parts)
    {
        status = Status(StatusCode::kUnimplemented, "malformed method name: \"" + headers.path + "\"");
    }
    else if (!methodTable_.HasService(parts->first))
    {
        status = Status(StatusCode::kUnimplemented, "unknown service " + std::string(parts->first));
    }
    else
    {
        status = Status(StatusCode::kUnimplemented, "unknown method " + std::string(parts->second) + " for service " +
                                                        std::string(parts->first));
    }
    const uint64_t occurrence = ++stats_.unimplemented;
    CountStatus(std::nullopt, status.code());
    if (ShouldLogServerEvent(occurrence))
    {
        LOG_WARN << "[GrpcServer] " << listenAddress_ << ": " << status.message() << " (occurrence " << occurrence
                 << ")";
    }
    return RejectVerdict(std::move(status));
}

ServerSession::Verdict Server::AcceptCall(detail::ServerConnection &connection, int32_t streamId,
                                          RequestHeaders headers, std::string message,
                                          std::chrono::steady_clock::time_point headersAt)
{
    if (phase_ == Phase::kDrainExpired || phase_ == Phase::kDrained)
    {
        ++stats_.refusedOnShutdown; // 排空到期之后才收完的流
        return RefuseVerdict();
    }
    const auto found = methodIndex_.find(std::string_view(headers.path));
    if (found == methodIndex_.end())
    {
        return RouteRequest(headers); // 不可达:头部阶段已路由
    }
    const size_t methodIndex = found->second;
    if (calls_.size() >= options_.maxInFlightCalls)
    {
        // 业务之前回 RESOURCE_EXHAUSTED(不用 REFUSED_STREAM:grpc-go 会立刻在同一连接透明重试),§15.3 Q2。
        const uint64_t occurrence = ++stats_.rejectedOverload;
        CountStatus(methodIndex, StatusCode::kResourceExhausted);
        if (ShouldLogServerEvent(occurrence))
        {
            LOG_WARN << "[GrpcServer] " << listenAddress_ << ": in-flight call limit (" << options_.maxInFlightCalls
                     << ") reached, rejecting " << methods_[methodIndex].displayName << " (occurrence " << occurrence
                     << ")";
        }
        return RejectVerdict(Status(StatusCode::kResourceExhausted, "server in-flight call limit (" +
                                                                        std::to_string(options_.maxInFlightCalls) +
                                                                        ") reached"));
    }

    const uint64_t callId = nextCallId_++;
    CallRecord record;
    record.connectionId = connection.id();
    record.streamId = streamId;
    record.methodIndex = methodIndex;
    record.peer = connection.peer();
    record.metadata = std::move(headers.metadata);
    if (headers.timeout)
    {
        record.deadline = DeadlineFrom(headersAt, *headers.timeout); // ServerSession 保证 timeout 为正
    }
    record.message = std::move(message);
    const auto deadline = record.deadline;
    calls_.emplace(callId, std::move(record));
    connection.BindStream(streamId, callId);
    stats_.inFlightPeak = std::max<uint64_t>(stats_.inFlightPeak, calls_.size());
    if (deadline)
    {
        deadlines_.emplace(*deadline, callId);
        ArmDeadlineTimer();
    }
    ready_.push_back(callId);
    stats_.maxBacklog = std::max<uint64_t>(stats_.maxBacklog, ready_.size());
    ScheduleDispatch();
    return AcceptVerdict();
}

void Server::AbortStream(detail::ServerConnection &connection, int32_t streamId)
{
    const auto callId = connection.TakeStream(streamId);
    if (!callId)
    {
        return;
    }
    // 对端 RST:不回应答。业务已开始的,之后的 Finish 计 late_finish(§15.5.6)。
    const auto phase = CompleteCall(*callId, CallEnd{});
    if (!phase)
    {
        return;
    }
    if (*phase == CallPhase::kStarted)
    {
        ++stats_.abortedAfterStart;
    }
    else
    {
        ++stats_.cancelledBeforeStart;
    }
}

void Server::OnSessionRejected(const detail::ServerConnection &connection, int httpStatus, const Status &status)
{
    CountStatus(std::nullopt, status.code());
    if (status.code() == StatusCode::kDeadlineExceeded)
    {
        // 会话层只在 grpc-timeout <= 0 时回这个码:截止在客户端发出前就已到,来自合法客户端,不是畸形请求。
        // 计入 deadline_before_start(§15.11),不计 rejected_protocol、不打 WARN(server_session.h OnRequestRejected)。
        ++stats_.deadlineBeforeStart;
        return;
    }
    const uint64_t occurrence = ++stats_.rejectedProtocol;
    if (ShouldLogServerEvent(occurrence))
    {
        LOG_WARN << "[GrpcServer] " << listenAddress_ << ": rejected a request from " << connection.peer()
                 << " before dispatch with HTTP " << httpStatus << " " << status.ToString() << " (occurrence "
                 << occurrence << ")";
    }
}

void Server::OnConnectionProtocolError(const detail::ServerConnection &connection, const Status &status)
{
    const uint64_t occurrence = ++stats_.protocolErrors;
    if (ShouldLogServerEvent(occurrence))
    {
        LOG_WARN << "[GrpcServer] " << listenAddress_ << ": closing connection from " << connection.peer()
                 << " after a protocol error: " << status.message() << " (occurrence " << occurrence << ")";
    }
}

void Server::OnHandshakeTimeout(const detail::ServerConnection &connection)
{
    ++stats_.handshakeTimeout;
    LOG_DEBUG << "[GrpcServer] " << listenAddress_ << ": HTTP/2 handshake with " << connection.peer()
              << " timed out after " << options_.handshakeTimeout.count() << "ms";
}

void Server::ScheduleDispatch()
{
    if (dispatchQueued_ || ready_.empty())
    {
        return;
    }
    dispatchQueued_ = true;
    // 泵任务经 queueInLoop 排队,同一时刻至多一个。泵内再排时 callingPendingFunctors_ 为真,会唤醒并落到下一轮,
    // 中间让出给定时器与 I/O(§15.6)。
    const std::weak_ptr<Server> weakSelf = weak_from_this();
    loop_.queueInLoop([weakSelf]
                      {
        if (const auto self = weakSelf.lock())
        {
            self->RunDispatchTurn();
        } });
}

void Server::RunDispatchTurn()
{
    dispatchQueued_ = false;
    BatchScope batch(*this); // 本轮产生的应答在泵结束时统一冲出
    const size_t maxPerTurn = std::max<size_t>(1, options_.maxDispatchPerTurn);
    const auto turnStart = SteadyClock::now();
    size_t processed = 0;
    uint64_t dispatchedThisTurn = 0;
    while (!ready_.empty() && processed < maxPerTurn)
    {
        if (processed > 0 && SteadyClock::now() - turnStart >= options_.dispatchTurnBudget)
        {
            break; // 每轮累计 ≤ 预算,至少处理 1 个
        }
        const uint64_t callId = ready_.front();
        ready_.pop_front();
        const auto it = calls_.find(callId);
        if (it == calls_.end() || it->second.phase != CallPhase::kQueued)
        {
            continue; // 排队期间已被撤销(对端 RST / 断连 / 截止)
        }
        ++processed;
        CallRecord &record = it->second;
        if (record.deadline && SteadyClock::now() >= *record.deadline)
        {
            CompleteCall(callId, CallEnd{CallEndKind::kStatus, std::string(),
                                         Status(StatusCode::kDeadlineExceeded,
                                                "deadline exceeded before the call started")});
            ++stats_.deadlineBeforeStart;
            continue;
        }
        record.phase = CallPhase::kDispatched;
        auto token = std::make_shared<detail::CallToken>();
        token->server = weak_from_this();
        token->callId = callId;
        token->loopThread = std::this_thread::get_id();
        token->method = methods_[record.methodIndex].path;
        token->peer = record.peer;
        token->metadata = std::move(record.metadata);
        token->deadline = record.deadline;
        // 先把 message 移出记录:handler 可能同步完成并删掉记录。methods_ 在 Start 后不再修改,引用稳定。
        const std::string message = std::move(record.message);
        const UnaryMethodHandler &handler = methods_[record.methodIndex].handler;
        ++stats_.dispatched;
        ++dispatchedThisTurn;
        handler(ServerCall(std::move(token)), std::string_view(message));
    }
    stats_.maxDispatchedPerTurn = std::max(stats_.maxDispatchedPerTurn, dispatchedThisTurn);
    if (!ready_.empty())
    {
        ScheduleDispatch();
    }
}

std::optional<Server::CallPhase> Server::CompleteCall(uint64_t callId, CallEnd end)
{
    // §15.7:找到即 erase —— 恰好一次的唯一裁决点。先移出记录并删表,再递减在途、提交应答、标记冲出;
    // 记录本身(请求体、metadata)在函数结束时才释放。
    const auto it = calls_.find(callId);
    if (it == calls_.end())
    {
        return std::nullopt;
    }
    CallRecord record = std::move(it->second);
    calls_.erase(it);
    if (record.deadline)
    {
        deadlines_.erase({*record.deadline, callId});
    }
    if (end.kind == CallEndKind::kResponse)
    {
        CountStatus(record.methodIndex, StatusCode::kOk);
    }
    else if (end.kind == CallEndKind::kStatus)
    {
        CountStatus(record.methodIndex, end.status.code());
    }
    const auto connectionIt = connections_.find(record.connectionId);
    if (connectionIt != connections_.end())
    {
        const auto connection = connectionIt->second.connection;
        connection->ForgetStream(record.streamId);
        if (end.kind != CallEndKind::kSilent)
        {
            connection->Respond(record.streamId, std::move(end));
            MarkConnectionDirty(record.connectionId);
        }
    }
    return record.phase;
}

bool Server::TryStartBusiness(uint64_t callId)
{
    const auto it = calls_.find(callId);
    if (it == calls_.end())
    {
        return false; // 已被截止 / 取消 / 排空收口
    }
    if (it->second.phase == CallPhase::kStarted)
    {
        return false; // 已开始过一次:不让业务执行两次
    }
    if (it->second.deadline && SteadyClock::now() >= *it->second.deadline)
    {
        // 判定「有效且未到期」与标记「已开始」在同一次 loop 调用里完成(§15.7 第 4 条)。
        CompleteCall(callId, CallEnd{CallEndKind::kStatus, std::string(),
                                     Status(StatusCode::kDeadlineExceeded,
                                            "deadline exceeded before the call started")});
        ++stats_.deadlineBeforeStart;
        return false;
    }
    it->second.phase = CallPhase::kStarted;
    return true;
}

void Server::FinishFromHandler(const detail::CallToken &token, CallEnd end)
{
    if (calls_.find(token.callId) == calls_.end())
    {
        const uint64_t occurrence = ++stats_.lateFinish;
        if (ShouldLogServerEvent(occurrence))
        {
            LOG_DEBUG << "[GrpcServer] late finish ignored (call already completed): method=" << token.method
                      << " call_id=" << token.callId << " (occurrence " << occurrence << ")";
        }
        return;
    }
    if (end.kind == CallEndKind::kResponse && end.response.size() > options_.maxSendMessageBytes)
    {
        // 发生在业务之后(§15.13):Go 客户端收包时本来也报同一个码。
        LOG_ERROR << "[GrpcServer] response of " << token.method << " is " << end.response.size()
                  << " bytes, larger than max " << options_.maxSendMessageBytes << "; replying RESOURCE_EXHAUSTED";
        const std::string message = "grpc: trying to send message larger than max (" +
                                    std::to_string(end.response.size()) + " vs. " +
                                    std::to_string(options_.maxSendMessageBytes) + ")";
        end = CallEnd{CallEndKind::kStatus, std::string(), Status(StatusCode::kResourceExhausted, message)};
    }
    CompleteCall(token.callId, std::move(end));
}

void Server::OnTokenDropped(const detail::CallToken &token)
{
    if (calls_.find(token.callId) == calls_.end())
    {
        return; // 正常路径:调用已完成
    }
    // 业务可能已执行,只能回「结果未知」码(§15.4.3)。
    const uint64_t occurrence = ++stats_.dropped;
    if (ShouldLogServerEvent(occurrence))
    {
        LOG_ERROR << "[GrpcServer] call dropped without completion: method=" << token.method
                  << " call_id=" << token.callId << "; replying UNAVAILABLE (occurrence " << occurrence << ")";
    }
    CompleteCall(token.callId, CallEnd{CallEndKind::kStatus, std::string(),
                                       Status(StatusCode::kUnavailable,
                                              "server dropped the call without completing it")});
}

void Server::CountStatus(std::optional<size_t> methodIndex, StatusCode code)
{
    const uint32_t value = static_cast<uint32_t>(code);
    ++stats_.codes[value];
    if (methodIndex && *methodIndex < stats_.methodCodes.size())
    {
        ++stats_.methodCodes[*methodIndex][value];
    }
}

void Server::ArmDeadlineTimer()
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
    // muduo 定时器按墙钟走,只当唤醒提示;到点后用 steady_clock 复核,未到就重新上弦(照 Channel)。
    // 加 1ms 余量,避免墙钟与单调钟的取整差让定时器早到一点而空转一轮。
    const std::weak_ptr<Server> weakSelf = weak_from_this();
    deadlineTimer_ = loop_.runAfter(ToSeconds(earliest - SteadyClock::now()) + 0.001, [weakSelf]
                                    {
        if (const auto self = weakSelf.lock())
        {
            self->OnDeadlineTimer();
        } });
    deadlineTimerArmed_ = true;
    deadlineTimerFor_ = earliest;
}

void Server::OnDeadlineTimer()
{
    deadlineTimerArmed_ = false;
    BatchScope batch(*this);
    const auto now = SteadyClock::now();
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
        const auto it = calls_.find(callId);
        if (it == calls_.end())
        {
            continue;
        }
        if (it->second.phase == CallPhase::kStarted)
        {
            // 业务已开始(只有延后完成的 handler 会处在这里):立即回,业务不打断,迟到的完成丢弃并计 late_finish。
            CompleteCall(callId, CallEnd{CallEndKind::kStatus, std::string(),
                                         Status(StatusCode::kDeadlineExceeded, "deadline exceeded")});
            ++stats_.deadlineAfterStart;
        }
        else
        {
            // 排队中或等许可:不派发 / 不进业务;许可回来后 TryStartBusiness 为假。
            CompleteCall(callId, CallEnd{CallEndKind::kStatus, std::string(),
                                         Status(StatusCode::kDeadlineExceeded,
                                                "deadline exceeded before the call started")});
            ++stats_.deadlineBeforeStart;
        }
    }
    ArmDeadlineTimer();
}

void Server::BeginShutdown(ServerShutdownOptions options, std::function<void()> onDrained)
{
    loop_.assertInLoopThread();
    if (phase_ != Phase::kCreated && phase_ != Phase::kRunning)
    {
        LOG_WARN << "[GrpcServer] " << listenAddress_ << ": BeginShutdown called again; ignored";
        return;
    }
    // a) 置排空:此后 ServerCall::ServerDraining() 为真。
    phase_ = Phase::kDraining;
    shutdownOptions_ = options;
    onDrained_ = std::move(onDrained);
    drainStartedAt_ = SteadyClock::now();
    drainDeadline_ = drainStartedAt_ + options.drainTimeout;
    LOG_INFO << "gRPC server draining: listen=" << listenAddress_ << " conns=" << connections_.size()
             << " inflight=" << calls_.size() << " drain_timeout_ms=" << options.drainTimeout.count();

    // b) 停听:Acceptor 移进 queueInLoop 闭包销毁。不能在这里同步销毁 —— 本调用可能在定时器回调里
    //    (Windows 的定时器跑在 eventHandling_ 期间,Linux 的 timerfd 本身就是活跃 channel),
    //    而 Acceptor 的 channel 可能同在本轮 activeChannels_ 里,removeChannel 会撞断言。
    //    销毁前到达的接入由 HandleNewConnection 一律 close。
    if (acceptor_)
    {
        loop_.queueInLoop([acceptor = std::move(acceptor_)]() mutable { acceptor.reset(); });
        acceptor_.reset();
    }

    {
        BatchScope batch(*this);
        // c) 每条连接:收到过字节 → 第一段 GOAWAY + PING;没收到过 → forceClose(只排队,不会同步摘表)。
        for (auto &entry : connections_)
        {
            if (!entry.second.connection->receivedAnyBytes())
            {
                entry.second.connection->ForceClose();
                continue;
            }
            entry.second.connection->BeginShutdown(options.goawayPingTimeout, options.closeGrace);
            MarkConnectionDirty(entry.first);
        }
    }

    // d) 排空定时器(steady 复核)。
    ArmDrainTimer();
    // e) 已满足排空条件(没有连接)→ queueInLoop(onDrained),永不在本栈内回调。
    MaybeFinishDrain();
}

void Server::ArmDrainTimer()
{
    const std::weak_ptr<Server> weakSelf = weak_from_this();
    drainTimer_ = loop_.runAfter(ToSeconds(drainDeadline_ - SteadyClock::now()) + 0.001, [weakSelf]
                                 {
        if (const auto self = weakSelf.lock())
        {
            self->OnDrainTimer();
        } });
    drainTimerArmed_ = true;
}

void Server::OnDrainTimer()
{
    drainTimerArmed_ = false;
    if (phase_ != Phase::kDraining)
    {
        return;
    }
    if (SteadyClock::now() < drainDeadline_)
    {
        ArmDrainTimer();
        return;
    }
    phase_ = Phase::kDrainExpired;
    uint64_t refused = 0;
    uint64_t unavailable = 0;
    {
        BatchScope batch(*this);
        std::vector<uint64_t> callIds;
        callIds.reserve(calls_.size());
        for (const auto &entry : calls_)
        {
            callIds.push_back(entry.first);
        }
        std::sort(callIds.begin(), callIds.end());
        for (const uint64_t callId : callIds)
        {
            const auto it = calls_.find(callId);
            if (it == calls_.end())
            {
                continue;
            }
            if (it->second.phase == CallPhase::kStarted)
            {
                // 业务已开始:结果未知(§15.3 Q4)。
                CompleteCall(callId, CallEnd{CallEndKind::kStatus, std::string(),
                                             Status(StatusCode::kUnavailable, "server shutting down")});
                ++stats_.unavailableOnShutdown;
                ++unavailable;
            }
            else
            {
                // 业务未开始:REFUSED_STREAM = RFC 9113 §8.7 的「未处理」,grpc-go 透明重试到新连接。
                CompleteCall(callId, CallEnd{CallEndKind::kRefuse, std::string(), Status()});
                ++stats_.refusedOnShutdown;
                ++refused;
            }
        }
        // 所有连接补最终 GOAWAY、冲出上面提交的 RST / trailers(不受背压阈值限制),然后写半关;宽限到期 forceClose。
        // 冲出发生在 ExpireDrain 里、写半关之前;本作用域结束时 FlushDirtyConnections 看到的已是写半关的连接,什么都不做。
        std::vector<std::shared_ptr<detail::ServerConnection>> remaining;
        remaining.reserve(connections_.size());
        for (const auto &entry : connections_)
        {
            remaining.push_back(entry.second.connection);
        }
        for (const auto &connection : remaining)
        {
            connection->ExpireDrain();
        }
    }
    LOG_WARN << "[GrpcServer] " << listenAddress_ << ": drain timeout after " << shutdownOptions_.drainTimeout.count()
             << "ms: refused=" << refused << " unavailable=" << unavailable << " connections=" << connections_.size();
}

void Server::MaybeFinishDrain()
{
    if ((phase_ != Phase::kDraining && phase_ != Phase::kDrainExpired) || !connections_.empty())
    {
        return;
    }
    phase_ = Phase::kDrained;
    if (drainTimerArmed_)
    {
        loop_.cancel(drainTimer_);
        drainTimerArmed_ = false;
    }
    LOG_DEBUG << "[GrpcServer] " << listenAddress_ << " drained after "
              << std::chrono::duration_cast<std::chrono::milliseconds>(SteadyClock::now() - drainStartedAt_).count()
              << "ms";
    if (onDrained_)
    {
        // 恰好一次,经 queueInLoop:FIFO 排在最后一个 connectDestroyed(close 回调先排)之后。
        std::function<void()> done;
        done.swap(onDrained_);
        loop_.queueInLoop(std::move(done));
    }
}

ServerStatsSnapshot Server::Stats() const
{
    loop_.assertInLoopThread();
    ServerStatsSnapshot snapshot;
    snapshot.listen = listenAddress_;
    snapshot.connections = connections_.size();
    snapshot.connectionsPeak = stats_.connectionsPeak;
    snapshot.accepted = stats_.accepted;
    snapshot.closedBeforePreface = stats_.closedBeforePreface;
    snapshot.handshakeTimeout = stats_.handshakeTimeout;
    snapshot.protocolErrors = stats_.protocolErrors;
    snapshot.inFlight = calls_.size();
    snapshot.inFlightPeak = stats_.inFlightPeak;
    snapshot.dispatched = stats_.dispatched;
    snapshot.maxBacklog = stats_.maxBacklog;
    snapshot.maxDispatchedPerTurn = stats_.maxDispatchedPerTurn;
    snapshot.rejectedOverload = stats_.rejectedOverload;
    snapshot.rejectedProtocol = stats_.rejectedProtocol;
    snapshot.unimplemented = stats_.unimplemented;
    snapshot.deadlineBeforeStart = stats_.deadlineBeforeStart;
    snapshot.deadlineAfterStart = stats_.deadlineAfterStart;
    snapshot.cancelledBeforeStart = stats_.cancelledBeforeStart;
    snapshot.abortedAfterStart = stats_.abortedAfterStart;
    snapshot.refusedOnShutdown = stats_.refusedOnShutdown;
    snapshot.unavailableOnShutdown = stats_.unavailableOnShutdown;
    snapshot.lateFinish = stats_.lateFinish;
    snapshot.dropped = stats_.dropped;
    snapshot.droppedOffLoop = DroppedOffLoopCounter().load(std::memory_order_relaxed);
    snapshot.codes = stats_.codes;
    for (size_t i = 0; i < methods_.size() && i < stats_.methodCodes.size(); ++i)
    {
        for (const auto &[code, count] : stats_.methodCodes[i])
        {
            snapshot.methods[methods_[i].displayName][code] += count;
        }
    }
    return snapshot;
}

void Server::LogStatsIfChanged()
{
    std::string current = FormatServerStats(Stats());
    if (current == lastLoggedStats_)
    {
        return;
    }
    lastLoggedStats_ = current;
    if (current.size() > kMaxSummaryLogBytes)
    {
        current.resize(kMaxSummaryLogBytes);
        current += " ...(truncated)";
    }
    LOG_INFO << "[GrpcServer] summary " << current;
}

void Server::CancelTimers()
{
    if (deadlineTimerArmed_)
    {
        loop_.cancel(deadlineTimer_);
        deadlineTimerArmed_ = false;
    }
    if (drainTimerArmed_)
    {
        loop_.cancel(drainTimer_);
        drainTimerArmed_ = false;
    }
    if (statsTimerArmed_)
    {
        loop_.cancel(statsTimer_);
        statsTimerArmed_ = false;
    }
}

std::string FormatServerStats(const ServerStatsSnapshot &stats)
{
    std::string out;
    out.reserve(640);
    const auto field = [&out](const char *name, uint64_t value)
    {
        out += ' ';
        out += name;
        out += '=';
        out += std::to_string(value);
    };
    const auto currentAndPeak = [&out](const char *name, uint64_t current, uint64_t peak)
    {
        out += ' ';
        out += name;
        out += '=';
        out += std::to_string(current);
        out += '/';
        out += std::to_string(peak);
    };
    out += "listen=";
    out += stats.listen;
    currentAndPeak("conns", stats.connections, stats.connectionsPeak);
    field("accepted", stats.accepted);
    field("closed_before_preface", stats.closedBeforePreface);
    field("handshake_timeout", stats.handshakeTimeout);
    field("protocol_errors", stats.protocolErrors);
    currentAndPeak("inflight", stats.inFlight, stats.inFlightPeak);
    field("dispatched", stats.dispatched);
    field("max_backlog", stats.maxBacklog);
    field("max_dispatched_per_turn", stats.maxDispatchedPerTurn);
    field("rejected_overload", stats.rejectedOverload);
    field("rejected_protocol", stats.rejectedProtocol);
    field("unimplemented", stats.unimplemented);
    field("deadline_before_start", stats.deadlineBeforeStart);
    field("deadline_after_start", stats.deadlineAfterStart);
    field("cancelled_before_start", stats.cancelledBeforeStart);
    field("aborted_after_start", stats.abortedAfterStart);
    field("refused_on_shutdown", stats.refusedOnShutdown);
    field("unavailable_on_shutdown", stats.unavailableOnShutdown);
    field("late_finish", stats.lateFinish);
    field("dropped", stats.dropped);
    field("dropped_off_loop", stats.droppedOffLoop);
    out += " codes=";
    bool first = true;
    for (const auto &[code, count] : stats.codes)
    {
        if (!first)
        {
            out += ',';
        }
        first = false;
        out += std::to_string(code);
        out += ':';
        out += std::to_string(count);
    }
    out += " methods={";
    first = true;
    for (const auto &[method, codes] : stats.methods)
    {
        for (const auto &[code, count] : codes)
        {
            if (!first)
            {
                out += ',';
            }
            first = false;
            out += method;
            out += ':';
            out += std::to_string(code);
            out += '=';
            out += std::to_string(count);
        }
    }
    out += '}';
    return out;
}

} // namespace muduo_grpc
