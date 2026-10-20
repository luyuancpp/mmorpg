#include "message_framing.h"

#include <algorithm>

namespace muduo_grpc
{

void AppendLengthPrefixedMessage(std::string &out, std::string_view payload)
{
    const auto length = static_cast<uint32_t>(payload.size());
    out.reserve(out.size() + kMessagePrefixBytes + payload.size());
    out.push_back('\0'); // 压缩标志:不压缩
    out.push_back(static_cast<char>((length >> 24) & 0xFF));
    out.push_back(static_cast<char>((length >> 16) & 0xFF));
    out.push_back(static_cast<char>((length >> 8) & 0xFF));
    out.push_back(static_cast<char>(length & 0xFF));
    out.append(payload.data(), payload.size());
}

Status MessageDeframer::Feed(const uint8_t *data, size_t len, std::vector<std::string> &out)
{
    if (!error_.ok())
    {
        return error_;
    }
    size_t offset = 0;
    while (offset < len)
    {
        if (bodyExpected_ == 0)
        {
            const size_t take = std::min(kMessagePrefixBytes - headerFilled_, len - offset);
            std::copy(data + offset, data + offset + take, header_ + headerFilled_);
            headerFilled_ += take;
            offset += take;
            if (headerFilled_ < kMessagePrefixBytes)
            {
                break;
            }
            headerFilled_ = 0;

            const uint8_t flag = header_[0];
            const uint32_t length = (static_cast<uint32_t>(header_[1]) << 24) |
                                    (static_cast<uint32_t>(header_[2]) << 16) |
                                    (static_cast<uint32_t>(header_[3]) << 8) |
                                    static_cast<uint32_t>(header_[4]);
            if (flag == 1)
            {
                error_ = Status(StatusCode::kInternal,
                                "received a compressed message but no message encoding was negotiated");
                return error_;
            }
            if (flag != 0)
            {
                error_ = Status(StatusCode::kInternal,
                                "invalid compressed-flag byte " + std::to_string(flag) + " in message prefix");
                return error_;
            }
            if (length > maxMessageBytes_)
            {
                error_ = Status(StatusCode::kResourceExhausted,
                                "received message larger than max (" + std::to_string(length) + " vs. " +
                                    std::to_string(maxMessageBytes_) + ")");
                return error_;
            }
            if (length == 0)
            {
                out.emplace_back(); // 空消息(如 base.Empty)也是一条消息
                continue;
            }
            bodyExpected_ = length;
            body_.clear();
            // 预分配封顶 64KiB:对端可以只报长度不发数据,按声明长度预分配会把每个流放大到上限(§15.4.2)。
            body_.reserve(std::min<size_t>(length, 64 * 1024));
            continue;
        }

        const size_t need = bodyExpected_ - body_.size();
        const size_t take = std::min(need, len - offset);
        body_.append(reinterpret_cast<const char *>(data + offset), take);
        offset += take;
        if (body_.size() == bodyExpected_)
        {
            out.push_back(std::move(body_));
            body_.clear();
            bodyExpected_ = 0;
        }
    }
    return Status::Ok();
}

} // namespace muduo_grpc
