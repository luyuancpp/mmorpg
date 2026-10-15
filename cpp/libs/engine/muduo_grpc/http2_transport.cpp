#include "http2_transport.h"

#include "muduo/base/Logging.h"
#include "muduo/net/Buffer.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpConnection.h"

namespace muduo_grpc
{

namespace
{

// HTTP/2 帧类型与错误码(RFC 9113)。本文件不 include nghttp2.h,只用到这几个常量。
constexpr uint8_t kFrameTypeData = 0x0;
constexpr uint8_t kFrameTypeHeaders = 0x1;

double ToSeconds(std::chrono::milliseconds duration)
{
    return static_cast<double>(duration.count()) / 1000.0;
}

} // namespace

std::shared_ptr<Http2Transport> Http2Transport::Create(muduo::net::EventLoop &loop,
                                                       const muduo::net::InetAddress &address,
                                                       const std::string &name, Http2TransportOptions options,
                                                       std::weak_ptr<Http2TransportListener> listener, uint64_t id)
{
    return std::make_shared<Http2Transport>(PrivateTag{}, loop, address, name, std::move(options),
                                            std::move(listener), id);
}

Http2Transport::Http2Transport(PrivateTag, muduo::net::EventLoop &loop, const muduo::net::InetAddress &address,
                               const std::string &name, Http2TransportOptions options,
                               std::weak_ptr<Http2TransportListener> listener, uint64_t id)
    : loop_(loop),
      client_(&loop, address, name),
      target_(address.toIpPort()),
      options_(std::move(options)),
      listener_(std::move(listener)),
      id_(id)
{
    // 不 enableRetry:一个对象只对应一次连接;重连与退避由 Channel 按 gRPC 规范做。
}

Http2Transport::~Http2Transport()
{
    CancelTimers();
    // §11.7:拆除前显式 forceClose,不依赖 ~TcpClient 的 use_count()==1 判断。
    // 取出的强引用只活在这个块里,~TcpClient 执行时已经释放。
    {
        const auto conn = client_.connection();
        if (conn)
        {
            conn->forceClose();
        }
    }
}

void Http2Transport::Connect()
{
    if (connectStarted_)
    {
        return;
    }
    connectStarted_ = true;
    if (weak_from_this().expired())
    {
        LOG_FATAL << "muduo_grpc::Http2Transport must be owned by shared_ptr before Connect()";
    }

    // muduo 会把回调拷到 TcpConnection 上,可能在本对象析构之后才触发(forceClose 一律排队):只绑 weak_ptr。
    std::weak_ptr<Http2Transport> weakSelf = weak_from_this();
    client_.setConnectionCallback([weakSelf](const muduo::net::TcpConnectionPtr &conn)
                                  {
        if (const auto self = weakSelf.lock())
        {
            self->HandleConnection(conn);
        } });
    client_.setMessageCallback([weakSelf](const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buffer,
                                          muduo::Timestamp)
                               {
        if (const auto self = weakSelf.lock())
        {
            self->HandleMessage(conn, buffer);
            return;
        }
        buffer->retrieveAll(); });
    client_.setWriteCompleteCallback([weakSelf](const muduo::net::TcpConnectionPtr &)
                                     {
        if (const auto self = weakSelf.lock())
        {
            self->FlushOutput();
        } });

    connectTimer_ = loop_.runAfter(ToSeconds(options_.connectTimeout), [weakSelf]
                                   {
        if (const auto self = weakSelf.lock())
        {
            self->HandleConnectTimeout();
        } });
    connectTimerArmed_ = true;
    client_.connect();
}

ClientSession::SubmitResult Http2Transport::StartUnaryStream(const UnaryRequestSpec &spec)
{
    loop_.assertInLoopThread();
    if (!ready() || draining_ || !session_)
    {
        ClientSession::SubmitResult result;
        result.error = Status(StatusCode::kUnavailable, "connection is not ready for new streams");
        result.retryOnNewConnection = true;
        return result;
    }
    auto result = session_->SubmitUnary(spec);
    if (result.retryOnNewConnection)
    {
        draining_ = true;
    }
    return result;
}

void Http2Transport::FlushOutput()
{
    if (closed_ || !session_)
    {
        return;
    }
    if (sessionDepth_ > 0)
    {
        flushDeferred_ = true;
        return;
    }
    const auto conn = connection_.lock();
    if (!conn || !conn->connected())
    {
        return;
    }
    if (conn->outputBuffer()->readableBytes() >= options_.maxPendingOutputBytes)
    {
        return; // 输出缓冲已满:等 writeComplete 回调再继续取帧,不让缓冲越过上限
    }
    flushDeferred_ = false;
    ++sessionDepth_;
    const Status status = session_->DrainOutput([this, &conn](std::string_view chunk)
                                                {
        conn->send(chunk.data(), static_cast<int>(chunk.size()));
        return conn->outputBuffer()->readableBytes() < options_.maxPendingOutputBytes; });
    --sessionDepth_;
    if (!status.ok())
    {
        CloseWithError(status);
        return;
    }
    if (flushDeferred_)
    {
        // 发送过程中又有新帧被提交(例如流结束回调里提交了 RST):再刷一轮。
        FlushOutput();
    }
}

void Http2Transport::ResetStream(int32_t streamId, uint32_t http2ErrorCode, Status status)
{
    if (closed_ || !session_)
    {
        return;
    }
    session_->ResetStream(streamId, http2ErrorCode, std::move(status));
    FlushOutput();
}

void Http2Transport::OnCallDeadlineExpired(std::chrono::steady_clock::time_point callStartedAt)
{
    if (!ready() || !session_ || probeOutstanding_)
    {
        return;
    }
    if (lastFrameReceivedAt_ > callStartedAt)
    {
        return; // 连接有动静,只是这个调用慢
    }
    // §4.4:只在「从没发过 PING」或「上个 PING 的 ACK 之后收到过 HEADERS / DATA」时发,
    // 这样按 grpc-go 默认的强制策略永远不会记 strike。
    if (pingEverSent_ && !headersOrDataAfterLastPingAck_)
    {
        return;
    }
    if (!session_->SubmitPing())
    {
        return;
    }
    pingEverSent_ = true;
    probeOutstanding_ = true;
    headersOrDataAfterLastPingAck_ = false;
    const uint64_t generation = ++probeGeneration_;
    std::weak_ptr<Http2Transport> weakSelf = weak_from_this();
    probeTimer_ = loop_.runAfter(ToSeconds(options_.keepaliveTimeout), [weakSelf, generation]
                                 {
        if (const auto self = weakSelf.lock())
        {
            self->HandleProbeTimeout(generation);
        } });
    probeTimerArmed_ = true;
    FlushOutput();
}

void Http2Transport::Shutdown()
{
    listener_.reset();
    if (!closed_)
    {
        closed_ = true;
        ready_ = false;
    }
    CancelTimers();
    CloseConnection();
}

void Http2Transport::HandleConnection(const muduo::net::TcpConnectionPtr &conn)
{
    if (conn->connected())
    {
        if (closed_)
        {
            // 已经放弃(握手超时之后才连上):不要这条连接。
            conn->forceClose();
            return;
        }
        connection_ = conn;
        conn->setTcpNoDelay(true);
        session_ = std::make_unique<ClientSession>(options_.session, *this);
        const Status started = session_->Start();
        if (!started.ok())
        {
            CloseWithError(started);
            return;
        }
        // 发出 client magic + SETTINGS;READY 要等对端 SETTINGS(Windows 的 win_connect 有假阳性)。
        FlushOutput();
        return;
    }

    connection_.reset();
    if (!closed_)
    {
        CloseWithError(Status(StatusCode::kUnavailable, ready_ ? "connection closed by peer"
                                                               : "connection closed during http2 handshake"));
    }
}

void Http2Transport::HandleMessage(const muduo::net::TcpConnectionPtr &, muduo::net::Buffer *buffer)
{
    if (closed_ || !session_)
    {
        buffer->retrieveAll();
        return;
    }
    // 每次读事件 muduo 只做一次 readv(≤ 可写空间 + 64KiB),这里同步解完,单轮 CPU 有界(§7)。
    ++sessionDepth_;
    const Status status =
        session_->Receive(reinterpret_cast<const uint8_t *>(buffer->peek()), buffer->readableBytes());
    --sessionDepth_;
    buffer->retrieveAll();
    if (!status.ok())
    {
        CloseWithError(status);
        return;
    }
    if (closed_)
    {
        return;
    }
    FlushOutput(); // WINDOW_UPDATE / PING ACK / SETTINGS ACK / 回调里提交的 RST
    if (!closed_ && session_->ShouldClose())
    {
        CloseWithError(Status(StatusCode::kUnavailable, "http2 session terminated"));
    }
}

void Http2Transport::HandleConnectTimeout()
{
    connectTimerArmed_ = false;
    if (ready_ || closed_)
    {
        return;
    }
    LOG_WARN << "muduo_grpc connect to " << target_ << " timed out after " << options_.connectTimeout.count()
             << "ms (no HTTP/2 SETTINGS from peer)";
    CloseWithError(Status(StatusCode::kUnavailable,
                          "connect timeout after " + std::to_string(options_.connectTimeout.count()) + "ms"));
}

void Http2Transport::HandleProbeTimeout(uint64_t probeGeneration)
{
    if (probeGeneration != probeGeneration_)
    {
        return;
    }
    probeTimerArmed_ = false;
    if (!probeOutstanding_ || closed_)
    {
        return;
    }
    LOG_WARN << "muduo_grpc connection to " << target_ << " did not acknowledge a liveness PING within "
             << options_.keepaliveTimeout.count() << "ms; treating it as dead";
    CloseWithError(Status(StatusCode::kUnavailable, "keepalive ping not acknowledged"));
}

void Http2Transport::CloseWithError(const Status &reason)
{
    if (closed_)
    {
        return;
    }
    if (sessionDepth_ > 0)
    {
        // 不在 nghttp2 的回调栈里拆:推迟到本轮回调结束。
        std::weak_ptr<Http2Transport> weakSelf = weak_from_this();
        loop_.queueInLoop([weakSelf, reason]
                          {
            if (const auto self = weakSelf.lock())
            {
                self->CloseWithError(reason);
            } });
        return;
    }
    // 若 nghttp2 因协议错误排了 GOAWAY,尽量随手发出(输出缓冲为空时 send 直接写 socket)。
    // 只尽力而为:这里不能走 FlushOutput,它失败时会回到 CloseWithError。
    if (session_)
    {
        if (const auto conn = connection_.lock(); conn && conn->connected())
        {
            ++sessionDepth_;
            (void)session_->DrainOutput([this, &conn](std::string_view chunk)
                                        {
                conn->send(chunk.data(), static_cast<int>(chunk.size()));
                return conn->outputBuffer()->readableBytes() < options_.maxPendingOutputBytes; });
            --sessionDepth_;
        }
    }
    closed_ = true;
    const bool wasReady = ready_;
    ready_ = false;
    CancelTimers();

    if (session_)
    {
        ++sessionDepth_;
        session_->FailAllStreams(reason);
        --sessionDepth_;
    }
    if (wasReady)
    {
        LOG_WARN << "muduo_grpc connection to " << target_ << " closed: " << reason.ToString();
    }
    if (const auto listener = listener_.lock())
    {
        listener->OnTransportClosed(*this, reason);
    }
    CloseConnection();
}

void Http2Transport::CloseConnection()
{
    const auto conn = client_.connection();
    if (conn)
    {
        conn->forceClose();
        return;
    }
    // 还没连上:停掉 Connector 的内部重试。只在没有连接时调用 stop() —— 有连接时 ~TcpClient 不会
    // 续命 Connector,而 stop() 排队的 stopInLoop 绑的是 Connector 裸指针(上游 FIXME)。
    if (connectStarted_)
    {
        client_.stop();
    }
}

void Http2Transport::CancelTimers()
{
    // 进程退出路径上 EventLoop 可能已不在本线程(或已析构):定时器随 loop 一起释放,闭包只持 weak_ptr。
    if (muduo::net::EventLoop::getEventLoopOfCurrentThread() != &loop_)
    {
        connectTimerArmed_ = false;
        probeTimerArmed_ = false;
        return;
    }
    if (connectTimerArmed_)
    {
        loop_.cancel(connectTimer_);
        connectTimerArmed_ = false;
    }
    if (probeTimerArmed_)
    {
        loop_.cancel(probeTimer_);
        probeTimerArmed_ = false;
    }
}

void Http2Transport::OnSessionReady()
{
    ready_ = true;
    if (connectTimerArmed_)
    {
        loop_.cancel(connectTimer_);
        connectTimerArmed_ = false;
    }
    if (const auto listener = listener_.lock())
    {
        listener->OnTransportReady(*this);
    }
}

void Http2Transport::OnGoawayReceived(int32_t lastStreamId, uint32_t errorCode, std::string_view debugData)
{
    draining_ = true;
    if (const auto listener = listener_.lock())
    {
        listener->OnTransportGoaway(*this, lastStreamId, errorCode, debugData);
    }
}

void Http2Transport::OnFrameReceived(uint8_t frameType)
{
    lastFrameReceivedAt_ = std::chrono::steady_clock::now();
    if ((frameType == kFrameTypeData || frameType == kFrameTypeHeaders) && pingEverSent_ && !probeOutstanding_)
    {
        headersOrDataAfterLastPingAck_ = true;
    }
}

void Http2Transport::OnPingAck()
{
    if (!probeOutstanding_)
    {
        return;
    }
    probeOutstanding_ = false;
    headersOrDataAfterLastPingAck_ = false;
    if (probeTimerArmed_)
    {
        loop_.cancel(probeTimer_);
        probeTimerArmed_ = false;
    }
}

void Http2Transport::OnStreamClosed(int32_t streamId, StreamOutcome outcome)
{
    if (const auto listener = listener_.lock())
    {
        listener->OnTransportStreamClosed(*this, streamId, std::move(outcome));
    }
}

} // namespace muduo_grpc
