#include "client_session.h"

#include <nghttp2/nghttp2.h>

#include <algorithm>
#include <cstring>
#include <iterator>

#include "muduo/base/Logging.h"

namespace muduo_grpc
{

namespace
{

constexpr std::string_view kGrpcContentType = "application/grpc";

// RFC 9113 §6.5.2:头部列表大小按「名 + 值 + 32」累计。
constexpr size_t kHeaderEntryOverheadBytes = 32;

// 存活探测 PING 的 8 字节负载,只用于日志里辨认;ACK 的匹配不依赖它(同一时刻至多一个探测在途)。
constexpr uint8_t kProbePingPayload[8] = {'m', 'u', 'd', 'u', 'o', 'g', 'r', 'p'};

std::string_view AsView(const uint8_t *data, size_t len)
{
    return std::string_view(reinterpret_cast<const char *>(data), len);
}

// application/grpc 本身,或其后紧跟 '+'(application/grpc+proto)或 ';'(参数)。
bool IsGrpcContentType(std::string_view value)
{
    if (value.size() < kGrpcContentType.size() || value.substr(0, kGrpcContentType.size()) != kGrpcContentType)
    {
        return false;
    }
    if (value.size() == kGrpcContentType.size())
    {
        return true;
    }
    const char next = value[kGrpcContentType.size()];
    return next == '+' || next == ';';
}

std::optional<int> ParseHttpStatus(std::string_view value)
{
    if (value.size() != 3)
    {
        return std::nullopt;
    }
    int status = 0;
    for (const char c : value)
    {
        if (c < '0' || c > '9')
        {
            return std::nullopt;
        }
        status = status * 10 + (c - '0');
    }
    return status;
}

nghttp2_nv MakeNv(const std::string &name, const std::string &value)
{
    nghttp2_nv nv;
    nv.name = reinterpret_cast<uint8_t *>(const_cast<char *>(name.data()));
    nv.value = reinterpret_cast<uint8_t *>(const_cast<char *>(value.data()));
    nv.namelen = name.size();
    nv.valuelen = value.size();
    nv.flags = NGHTTP2_NV_FLAG_NONE; // nghttp2 自己拷贝名与值,调用返回后 name / value 即可释放
    return nv;
}

} // namespace

// nghttp2 回调。全部只在 ClientSession 的方法调用栈里被 nghttp2 调用(mem_recv2 / mem_send2 / submit_*)。
struct ClientSessionCallbacks
{
    using Block = ClientSession::StreamState::Block;

    static ClientSession &Self(void *userData) { return *static_cast<ClientSession *>(userData); }

    static int OnBeginHeaders(nghttp2_session *, const nghttp2_frame *frame, void *userData)
    {
        if (frame->hd.type != NGHTTP2_HEADERS)
        {
            return 0;
        }
        auto *stream = Self(userData).FindStream(frame->hd.stream_id);
        if (stream == nullptr)
        {
            return 0;
        }
        const bool endStream = (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) != 0;
        stream->blockBytes = 0;
        // 首个 HEADERS 是响应头;1xx 之后的最终响应头 nghttp2 归到 HCAT_HEADERS,用「还没收到响应头」区分。
        const bool isResponseBlock =
            frame->headers.cat == NGHTTP2_HCAT_RESPONSE || !stream->responseHeadersReceived;
        if (isResponseBlock)
        {
            stream->block = endStream ? Block::kTrailersOnly : Block::kResponseHeaders;
        }
        else
        {
            stream->block = Block::kTrailers;
        }
        return 0;
    }

    static int OnHeader(nghttp2_session *, const nghttp2_frame *frame, const uint8_t *nameData, size_t nameLen,
                        const uint8_t *valueData, size_t valueLen, uint8_t, void *userData)
    {
        if (frame->hd.type != NGHTTP2_HEADERS)
        {
            return 0;
        }
        auto &self = Self(userData);
        const int32_t streamId = frame->hd.stream_id;
        auto *stream = self.FindStream(streamId);
        if (stream == nullptr || stream->block == Block::kNone)
        {
            return 0;
        }

        stream->blockBytes += nameLen + valueLen + kHeaderEntryOverheadBytes;
        if (stream->blockBytes > self.options_.maxHeaderListBytes)
        {
            if (!stream->localStatus)
            {
                stream->localStatus = Status(StatusCode::kResourceExhausted,
                                             "received header list larger than max " +
                                                 std::to_string(self.options_.maxHeaderListBytes) + " bytes");
            }
            // nghttp2 以 RST_STREAM(INTERNAL_ERROR) 关流;本块剩余头部不再回调。
            return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
        }

        const std::string_view name = AsView(nameData, nameLen);
        const std::string_view value = AsView(valueData, valueLen);

        if (name == ":status")
        {
            const auto status = ParseHttpStatus(value);
            stream->httpStatus = status.value_or(0);
            return 0;
        }
        if (!name.empty() && name.front() == ':')
        {
            return 0; // 其他伪头与 gRPC 无关
        }
        if (stream->httpStatus >= 100 && stream->httpStatus < 200)
        {
            return 0; // 1xx 非最终响应:其中的头部不是 gRPC metadata
        }
        if (name == "content-type")
        {
            if (stream->block != Block::kTrailers)
            {
                stream->contentTypeIsGrpc = IsGrpcContentType(value);
            }
            return 0;
        }
        if (name == "grpc-status")
        {
            stream->grpcStatusPresent = true;
            stream->grpcStatus = ParseGrpcStatusValue(value);
            return 0;
        }
        if (name == "grpc-message")
        {
            stream->grpcMessage = PercentDecodeGrpcMessage(value);
            return 0;
        }
        if (name == "grpc-encoding" || name == "grpc-accept-encoding")
        {
            return 0; // 压缩由消息前缀的标志位判定(收到压缩消息即违约,见 MessageDeframer)
        }

        Metadata &target = stream->block == Block::kResponseHeaders ? stream->initialMetadata : stream->trailingMetadata;
        if (IsBinaryHeaderKey(name))
        {
            auto decoded = DecodeBinaryHeaderValue(value);
            if (!decoded)
            {
                if (!stream->localStatus)
                {
                    stream->localStatus = Status(StatusCode::kInternal,
                                                 "invalid base64 in binary metadata '" + std::string(name) + "'");
                }
                return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
            }
            target.emplace_back(std::string(name), std::move(*decoded));
        }
        else
        {
            target.emplace_back(std::string(name), std::string(value));
        }
        return 0;
    }

    static int OnFrameRecv(nghttp2_session *, const nghttp2_frame *frame, void *userData)
    {
        auto &self = Self(userData);
        self.observer_.OnFrameReceived(frame->hd.type);
        switch (frame->hd.type)
        {
        case NGHTTP2_SETTINGS:
            if ((frame->hd.flags & NGHTTP2_FLAG_ACK) == 0 && !self.readySignaled_)
            {
                self.readySignaled_ = true;
                self.observer_.OnSessionReady();
            }
            break;
        case NGHTTP2_HEADERS:
        {
            auto *stream = self.FindStream(frame->hd.stream_id);
            if (stream == nullptr)
            {
                break;
            }
            const bool informational = stream->httpStatus >= 100 && stream->httpStatus < 200;
            switch (stream->block)
            {
            case Block::kResponseHeaders:
                if (!informational)
                {
                    stream->responseHeadersReceived = true;
                }
                break;
            case Block::kTrailersOnly:
                stream->responseHeadersReceived = true;
                stream->trailersReceived = true;
                break;
            case Block::kTrailers:
                stream->trailersReceived = true;
                break;
            case Block::kNone:
                break;
            }
            if (informational && stream->block == Block::kResponseHeaders)
            {
                stream->httpStatus = 0; // 等最终响应头重新给出
            }
            stream->block = Block::kNone;
            break;
        }
        case NGHTTP2_PING:
            if ((frame->hd.flags & NGHTTP2_FLAG_ACK) != 0)
            {
                self.observer_.OnPingAck();
            }
            break;
        case NGHTTP2_GOAWAY:
            self.goawayReceived_ = true;
            self.observer_.OnGoawayReceived(frame->goaway.last_stream_id, frame->goaway.error_code,
                                            AsView(frame->goaway.opaque_data, frame->goaway.opaque_data_len));
            break;
        default:
            break;
        }
        return 0;
    }

    static int OnFrameSend(nghttp2_session *, const nghttp2_frame *frame, void *userData)
    {
        if (frame->hd.type == NGHTTP2_HEADERS && frame->headers.cat == NGHTTP2_HCAT_REQUEST)
        {
            if (auto *stream = Self(userData).FindStream(frame->hd.stream_id))
            {
                stream->requestHeadersSent = true;
            }
        }
        return 0;
    }

    static int OnDataChunkRecv(nghttp2_session *, uint8_t, int32_t streamId, const uint8_t *data, size_t len,
                               void *userData)
    {
        auto &self = Self(userData);
        auto *stream = self.FindStream(streamId);
        if (stream == nullptr || stream->localStatus)
        {
            return 0; // 本端已经放弃这个流(截止 / 超限):数据只计入流控,直接丢弃
        }
        if (stream->httpStatus != 200 || !stream->contentTypeIsGrpc)
        {
            // 不是 gRPC 响应(如代理回的 503 + text/html):响应体不是长度前缀消息,不拆;
            // 结果按 HTTP 状态合成(§4.3),与 grpc-go 一致。
            return 0;
        }
        Status status = stream->deframer.Feed(data, len, stream->messages);
        if (!status.ok())
        {
            self.SetLocalErrorAndReset(streamId, *stream, std::move(status), NGHTTP2_CANCEL);
            return 0;
        }
        if (stream->messages.size() > 1)
        {
            self.SetLocalErrorAndReset(
                streamId, *stream,
                Status(StatusCode::kInternal, "received more than one response message for a unary call"),
                NGHTTP2_CANCEL);
        }
        return 0;
    }

    static int OnStreamClose(nghttp2_session *, int32_t streamId, uint32_t errorCode, void *userData)
    {
        auto &self = Self(userData);
        auto it = self.streams_.find(streamId);
        if (it == self.streams_.end())
        {
            return 0;
        }
        std::unique_ptr<ClientSession::StreamState> stream = std::move(it->second);
        self.streams_.erase(it);
        StreamOutcome outcome = self.BuildOutcome(*stream, errorCode);
        self.observer_.OnStreamClosed(streamId, std::move(outcome));
        return 0;
    }

    static int OnError(nghttp2_session *, int libErrorCode, const char *msg, size_t len, void *)
    {
        LOG_DEBUG << "nghttp2 client session error " << libErrorCode << ": " << std::string(msg, len);
        return 0;
    }

    static nghttp2_ssize ReadRequestBody(nghttp2_session *, int32_t streamId, uint8_t *buf, size_t length,
                                         uint32_t *dataFlags, nghttp2_data_source *, void *userData)
    {
        auto *stream = Self(userData).FindStream(streamId);
        if (stream == nullptr || !stream->framedRequest)
        {
            *dataFlags |= NGHTTP2_DATA_FLAG_EOF;
            return 0;
        }
        const std::string &body = *stream->framedRequest;
        const size_t remaining = body.size() - stream->sentOffset;
        const size_t chunk = std::min(remaining, length);
        if (chunk > 0)
        {
            std::memcpy(buf, body.data() + stream->sentOffset, chunk);
        }
        stream->sentOffset += chunk;
        if (stream->sentOffset == body.size())
        {
            *dataFlags |= NGHTTP2_DATA_FLAG_EOF; // 请求体发完即 END_STREAM(unary 半关闭)
        }
        return static_cast<nghttp2_ssize>(chunk);
    }
};

void ClientSession::SessionDeleter::operator()(nghttp2_session *session) const
{
    nghttp2_session_del(session);
}

ClientSession::ClientSession(ClientSessionOptions options, Observer &observer)
    : options_(std::move(options)), observer_(observer)
{
}

ClientSession::~ClientSession() = default;

Status ClientSession::Start()
{
    nghttp2_session_callbacks *rawCallbacks = nullptr;
    if (nghttp2_session_callbacks_new(&rawCallbacks) != 0)
    {
        return Status(StatusCode::kResourceExhausted, "nghttp2_session_callbacks_new failed");
    }
    const std::unique_ptr<nghttp2_session_callbacks, void (*)(nghttp2_session_callbacks *)> callbacks(
        rawCallbacks, &nghttp2_session_callbacks_del);
    nghttp2_session_callbacks_set_on_begin_headers_callback(callbacks.get(), &ClientSessionCallbacks::OnBeginHeaders);
    nghttp2_session_callbacks_set_on_header_callback(callbacks.get(), &ClientSessionCallbacks::OnHeader);
    nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks.get(), &ClientSessionCallbacks::OnFrameRecv);
    nghttp2_session_callbacks_set_on_frame_send_callback(callbacks.get(), &ClientSessionCallbacks::OnFrameSend);
    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks.get(),
                                                              &ClientSessionCallbacks::OnDataChunkRecv);
    nghttp2_session_callbacks_set_on_stream_close_callback(callbacks.get(), &ClientSessionCallbacks::OnStreamClose);
    nghttp2_session_callbacks_set_error_callback2(callbacks.get(), &ClientSessionCallbacks::OnError);

    nghttp2_option *rawOption = nullptr;
    if (nghttp2_option_new(&rawOption) != 0)
    {
        return Status(StatusCode::kResourceExhausted, "nghttp2_option_new failed");
    }
    const std::unique_ptr<nghttp2_option, void (*)(nghttp2_option *)> option(rawOption, &nghttp2_option_del);
    // 其余保持默认:自动回 PING ACK、自动 WINDOW_UPDATE、开启 HTTP 语义校验(响应必须有 :status 等)。
    // 关掉 RFC 9113 的首尾空白校验:grpc-message 的空格不做百分号编码,grpc-go 发出 "error: " 这类
    // 以空格结尾的消息时,nghttp2 默认会把这个头当非法字段丢掉,状态说明就没了(状态码不受影响)。
    nghttp2_option_set_no_rfc9113_leading_and_trailing_ws_validation(option.get(), 1);

    nghttp2_session *rawSession = nullptr;
    const int created = nghttp2_session_client_new2(&rawSession, callbacks.get(), this, option.get());
    if (created != 0)
    {
        return Status(StatusCode::kResourceExhausted,
                      std::string("nghttp2_session_client_new2 failed: ") + nghttp2_strerror(created));
    }
    session_.reset(rawSession);

    const nghttp2_settings_entry settings[] = {
        {NGHTTP2_SETTINGS_ENABLE_PUSH, 0},
        {NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE, options_.initialStreamWindowBytes},
        {NGHTTP2_SETTINGS_MAX_HEADER_LIST_SIZE, options_.maxHeaderListBytes},
    };
    int rv = nghttp2_submit_settings(session_.get(), NGHTTP2_FLAG_NONE, settings, std::size(settings));
    if (rv != 0)
    {
        return Status(StatusCode::kInternal, std::string("nghttp2_submit_settings failed: ") + nghttp2_strerror(rv));
    }
    // 连接级接收窗口默认 65535,放大以免多个并发响应互相卡流控。
    rv = nghttp2_session_set_local_window_size(session_.get(), NGHTTP2_FLAG_NONE, 0,
                                               static_cast<int32_t>(options_.connectionWindowBytes));
    if (rv != 0)
    {
        return Status(StatusCode::kInternal,
                      std::string("nghttp2_session_set_local_window_size failed: ") + nghttp2_strerror(rv));
    }
    return Status::Ok();
}

ClientSession::SubmitResult ClientSession::SubmitUnary(const UnaryRequestSpec &spec)
{
    SubmitResult result;
    if (!session_)
    {
        result.error = Status(StatusCode::kInternal, "http2 session not started");
        return result;
    }
    if (goawayReceived_)
    {
        result.error = Status(StatusCode::kUnavailable, "connection is draining (GOAWAY received)");
        result.retryOnNewConnection = true;
        return result;
    }
    if (!spec.framedMessage)
    {
        result.error = Status(StatusCode::kInternal, "request message missing");
        return result;
    }

    // 先把全部头部拼成稳定的字符串,再建 nghttp2_nv 引用它们(vector 不再扩容,指针稳定)。
    std::vector<std::pair<std::string, std::string>> headers;
    headers.reserve(8 + spec.metadata.size());
    headers.emplace_back(":method", "POST");
    headers.emplace_back(":scheme", "http");
    headers.emplace_back(":path", spec.path);
    headers.emplace_back(":authority", options_.authority);
    headers.emplace_back("te", "trailers");
    if (!spec.grpcTimeout.empty())
    {
        headers.emplace_back("grpc-timeout", spec.grpcTimeout);
    }
    headers.emplace_back("content-type", std::string(kGrpcContentType));
    headers.emplace_back("user-agent", options_.userAgent);
    for (const auto &[key, value] : spec.metadata)
    {
        if (!IsValidCustomMetadataKey(key))
        {
            result.error = Status(StatusCode::kInternal, "invalid metadata key '" + key + "'");
            return result;
        }
        if (IsBinaryHeaderKey(key))
        {
            headers.emplace_back(key, Base64EncodeUnpadded(value));
        }
        else
        {
            if (!IsValidAsciiMetadataValue(value))
            {
                result.error = Status(StatusCode::kInternal, "invalid ASCII value for metadata key '" + key + "'");
                return result;
            }
            headers.emplace_back(key, value);
        }
    }

    std::vector<nghttp2_nv> nva;
    nva.reserve(headers.size());
    for (const auto &[name, value] : headers)
    {
        nva.push_back(MakeNv(name, value));
    }

    auto stream = std::make_unique<StreamState>(options_.maxReceiveMessageBytes);
    stream->framedRequest = spec.framedMessage;

    nghttp2_data_provider2 provider;
    provider.source.ptr = nullptr;
    provider.read_callback = &ClientSessionCallbacks::ReadRequestBody;

    const int32_t streamId =
        nghttp2_submit_request2(session_.get(), nullptr, nva.data(), nva.size(), &provider, nullptr);
    if (streamId < 0)
    {
        if (streamId == NGHTTP2_ERR_STREAM_ID_NOT_AVAILABLE)
        {
            result.error = Status(StatusCode::kUnavailable, "stream ids exhausted on this connection");
            result.retryOnNewConnection = true;
            return result;
        }
        result.error =
            Status(StatusCode::kInternal, std::string("failed to submit request: ") + nghttp2_strerror(streamId));
        return result;
    }
    streams_.emplace(streamId, std::move(stream));
    result.streamId = streamId;
    return result;
}

void ClientSession::ResetStream(int32_t streamId, uint32_t http2ErrorCode, Status status)
{
    auto *stream = FindStream(streamId);
    if (stream == nullptr || !session_)
    {
        return;
    }
    SetLocalErrorAndReset(streamId, *stream, std::move(status), http2ErrorCode);
}

Status ClientSession::Receive(const uint8_t *data, size_t len)
{
    if (!session_)
    {
        return Status(StatusCode::kInternal, "http2 session not started");
    }
    const nghttp2_ssize consumed = nghttp2_session_mem_recv2(session_.get(), data, len);
    if (consumed < 0)
    {
        return Status(StatusCode::kUnavailable, std::string("http2 receive failed: ") +
                                                    nghttp2_strerror(static_cast<int>(consumed)));
    }
    return Status::Ok();
}

Status ClientSession::DrainOutput(const std::function<bool(std::string_view chunk)> &write)
{
    if (!session_)
    {
        return Status(StatusCode::kInternal, "http2 session not started");
    }
    for (;;)
    {
        const uint8_t *data = nullptr;
        const nghttp2_ssize produced = nghttp2_session_mem_send2(session_.get(), &data);
        if (produced < 0)
        {
            return Status(StatusCode::kUnavailable,
                          std::string("http2 send failed: ") + nghttp2_strerror(static_cast<int>(produced)));
        }
        if (produced == 0)
        {
            return Status::Ok();
        }
        if (!write(AsView(data, static_cast<size_t>(produced))))
        {
            return Status::Ok();
        }
    }
}

bool ClientSession::ShouldClose() const
{
    if (!session_)
    {
        return true;
    }
    return nghttp2_session_want_read(session_.get()) == 0 && nghttp2_session_want_write(session_.get()) == 0;
}

bool ClientSession::SubmitPing()
{
    return session_ && nghttp2_submit_ping(session_.get(), NGHTTP2_FLAG_NONE, kProbePingPayload) == 0;
}

void ClientSession::SubmitGoaway(uint32_t http2ErrorCode)
{
    if (session_)
    {
        nghttp2_session_terminate_session(session_.get(), http2ErrorCode);
    }
}

void ClientSession::FailAllStreams(const Status &status)
{
    auto streams = std::move(streams_);
    streams_.clear();
    for (auto &[streamId, stream] : streams)
    {
        StreamOutcome outcome;
        if (stream->localStatus)
        {
            outcome.status = *stream->localStatus;
        }
        else
        {
            outcome.status = status;
            // 请求头都没交给 socket,服务端不可能见过这个请求。
            outcome.unprocessed = !stream->requestHeadersSent;
        }
        observer_.OnStreamClosed(streamId, std::move(outcome));
    }
}

ClientSession::StreamState *ClientSession::FindStream(int32_t streamId)
{
    const auto it = streams_.find(streamId);
    return it == streams_.end() ? nullptr : it->second.get();
}

void ClientSession::SetLocalErrorAndReset(int32_t streamId, StreamState &stream, Status status,
                                          uint32_t http2ErrorCode)
{
    if (!stream.localStatus)
    {
        stream.localStatus = std::move(status);
    }
    stream.messages.clear();
    nghttp2_submit_rst_stream(session_.get(), NGHTTP2_FLAG_NONE, streamId, http2ErrorCode);
}

StreamOutcome ClientSession::BuildOutcome(StreamState &stream, uint32_t http2ErrorCode) const
{
    StreamOutcome outcome;
    if (stream.localStatus)
    {
        outcome.status = *stream.localStatus;
        return outcome;
    }
    if (http2ErrorCode == NGHTTP2_REFUSED_STREAM)
    {
        // 对端明确没处理(GOAWAY 之后的流、HEADERS 因 GOAWAY 没发出、或服务端主动 REFUSED_STREAM)。
        outcome.status = Status(StatusCode::kUnavailable, "stream refused by peer before processing");
        outcome.unprocessed = true;
        return outcome;
    }

    outcome.initialMetadata = std::move(stream.initialMetadata);
    outcome.trailingMetadata = std::move(stream.trailingMetadata);
    const bool isGrpcResponse = stream.httpStatus == 200 && stream.contentTypeIsGrpc;

    if (stream.trailersReceived)
    {
        if (!isGrpcResponse)
        {
            outcome.status = stream.httpStatus == 200
                                 ? Status(StatusCode::kUnknown, "response content-type is not application/grpc")
                                 : Status(StatusCodeFromHttpStatus(stream.httpStatus),
                                          "unexpected HTTP status " + std::to_string(stream.httpStatus));
            return outcome;
        }
        if (!stream.grpcStatusPresent)
        {
            outcome.status = Status(StatusCode::kUnknown, "missing grpc-status in trailers");
            return outcome;
        }
        if (!stream.grpcStatus)
        {
            outcome.status = Status(StatusCode::kUnknown, "invalid grpc-status in trailers");
            return outcome;
        }
        outcome.status = Status(*stream.grpcStatus, std::move(stream.grpcMessage));
        if (outcome.status.ok())
        {
            if (stream.deframer.HasPartialMessage())
            {
                outcome.status = Status(StatusCode::kInternal, "response ended in the middle of a message");
            }
            else if (stream.messages.size() != 1)
            {
                outcome.status =
                    Status(StatusCode::kInternal, "unary response must contain exactly one message, got " +
                                                      std::to_string(stream.messages.size()));
            }
            else
            {
                outcome.response = std::move(stream.messages.front());
            }
        }
        return outcome;
    }

    if (http2ErrorCode != NGHTTP2_NO_ERROR)
    {
        outcome.status = StatusFromRstStreamError(http2ErrorCode);
        return outcome;
    }
    if (stream.responseHeadersReceived && !isGrpcResponse)
    {
        outcome.status = stream.httpStatus == 200
                             ? Status(StatusCode::kUnknown, "response content-type is not application/grpc")
                             : Status(StatusCodeFromHttpStatus(stream.httpStatus),
                                      "unexpected HTTP status " + std::to_string(stream.httpStatus));
        return outcome;
    }
    outcome.status = Status(StatusCode::kInternal, "server closed the stream without sending trailers");
    return outcome;
}

} // namespace muduo_grpc
