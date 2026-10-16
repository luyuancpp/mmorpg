#include "metadata.h"

#include <array>
#include <cstdint>

namespace muduo_grpc
{

namespace
{

constexpr char kBase64Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
constexpr char kHexUpper[] = "0123456789ABCDEF";

// 256 项反查表:合法字符 → 0..63,其余 → -1。constexpr 生成,避免运行期初始化顺序问题。
constexpr std::array<int8_t, 256> MakeBase64ReverseTable()
{
    std::array<int8_t, 256> table{};
    for (auto &entry : table)
    {
        entry = -1;
    }
    for (int i = 0; i < 64; ++i)
    {
        table[static_cast<unsigned char>(kBase64Alphabet[i])] = static_cast<int8_t>(i);
    }
    return table;
}

constexpr std::array<int8_t, 256> kBase64Reverse = MakeBase64ReverseTable();

int HexDigitValue(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }
    return -1;
}

bool StartsWith(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

} // namespace

const std::string *FindMetadata(const Metadata &metadata, std::string_view key)
{
    for (const auto &[name, value] : metadata)
    {
        if (name == key)
        {
            return &value;
        }
    }
    return nullptr;
}

bool IsBinaryHeaderKey(std::string_view key)
{
    constexpr std::string_view kBinSuffix = "-bin";
    return key.size() > kBinSuffix.size() && key.substr(key.size() - kBinSuffix.size()) == kBinSuffix;
}

bool IsValidCustomMetadataKey(std::string_view key)
{
    if (key.empty() || key.front() == ':' || StartsWith(key, "grpc-"))
    {
        return false;
    }
    for (const char c : key)
    {
        const bool allowed = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || c == '_' || c == '.' || c == '-';
        if (!allowed)
        {
            return false;
        }
    }
    // HTTP/2 禁止的连接级头(RFC 9113 §8.2.2)与本层自己写的保留头。
    static constexpr std::string_view kReserved[] = {
        "connection", "keep-alive", "proxy-connection", "transfer-encoding", "upgrade", "host",
        "te", "content-type", "user-agent",
    };
    for (const auto reserved : kReserved)
    {
        if (key == reserved)
        {
            return false;
        }
    }
    return true;
}

bool IsValidAsciiMetadataValue(std::string_view value)
{
    for (const char c : value)
    {
        const auto byte = static_cast<unsigned char>(c);
        if (byte < 0x20 || byte > 0x7E)
        {
            return false;
        }
    }
    return true;
}

std::string Base64EncodeUnpadded(std::string_view input)
{
    std::string out;
    out.reserve((input.size() + 2) / 3 * 4);
    size_t i = 0;
    while (i + 3 <= input.size())
    {
        const uint32_t triple = (static_cast<uint32_t>(static_cast<unsigned char>(input[i])) << 16) |
                                (static_cast<uint32_t>(static_cast<unsigned char>(input[i + 1])) << 8) |
                                static_cast<uint32_t>(static_cast<unsigned char>(input[i + 2]));
        out.push_back(kBase64Alphabet[(triple >> 18) & 0x3F]);
        out.push_back(kBase64Alphabet[(triple >> 12) & 0x3F]);
        out.push_back(kBase64Alphabet[(triple >> 6) & 0x3F]);
        out.push_back(kBase64Alphabet[triple & 0x3F]);
        i += 3;
    }
    const size_t rest = input.size() - i;
    if (rest == 1)
    {
        const uint32_t single = static_cast<uint32_t>(static_cast<unsigned char>(input[i])) << 16;
        out.push_back(kBase64Alphabet[(single >> 18) & 0x3F]);
        out.push_back(kBase64Alphabet[(single >> 12) & 0x3F]);
    }
    else if (rest == 2)
    {
        const uint32_t pair = (static_cast<uint32_t>(static_cast<unsigned char>(input[i])) << 16) |
                              (static_cast<uint32_t>(static_cast<unsigned char>(input[i + 1])) << 8);
        out.push_back(kBase64Alphabet[(pair >> 18) & 0x3F]);
        out.push_back(kBase64Alphabet[(pair >> 12) & 0x3F]);
        out.push_back(kBase64Alphabet[(pair >> 6) & 0x3F]);
    }
    return out;
}

std::optional<std::string> Base64DecodeLenient(std::string_view input)
{
    // 带 padding 时整体长度必须是 4 的倍数且最多两个 '=';与 grpc-go 一致:
    // 长度是 4 的倍数按带 padding 解,否则按不带 padding 解。
    size_t padding = 0;
    while (padding < input.size() && padding < 2 && input[input.size() - 1 - padding] == '=')
    {
        ++padding;
    }
    if (padding > 0 && input.size() % 4 != 0)
    {
        return std::nullopt;
    }
    const std::string_view body = input.substr(0, input.size() - padding);
    if (body.size() % 4 == 1)
    {
        return std::nullopt;
    }

    std::string out;
    out.reserve(body.size() / 4 * 3 + 2);
    uint32_t accumulator = 0;
    int bits = 0;
    for (const char c : body)
    {
        const int8_t value = kBase64Reverse[static_cast<unsigned char>(c)];
        if (value < 0)
        {
            return std::nullopt; // 含 '='(出现在中间)或非字母表字符
        }
        accumulator = (accumulator << 6) | static_cast<uint32_t>(value);
        bits += 6;
        if (bits >= 8)
        {
            bits -= 8;
            out.push_back(static_cast<char>((accumulator >> bits) & 0xFF));
        }
    }
    return out;
}

std::optional<std::string> DecodeBinaryHeaderValue(std::string_view wireValue)
{
    std::string out;
    size_t start = 0;
    while (start <= wireValue.size())
    {
        const size_t comma = wireValue.find(',', start);
        const size_t end = comma == std::string_view::npos ? wireValue.size() : comma;
        std::string_view piece = wireValue.substr(start, end - start);
        // 合并时 "a, b" 形式的逗号后可能有空格。
        while (!piece.empty() && piece.front() == ' ')
        {
            piece.remove_prefix(1);
        }
        while (!piece.empty() && piece.back() == ' ')
        {
            piece.remove_suffix(1);
        }
        auto decoded = Base64DecodeLenient(piece);
        if (!decoded)
        {
            return std::nullopt;
        }
        out += *decoded;
        if (comma == std::string_view::npos)
        {
            break;
        }
        start = comma + 1;
    }
    return out;
}

std::string PercentEncodeGrpcMessage(std::string_view message)
{
    std::string out;
    out.reserve(message.size());
    for (const char c : message)
    {
        const auto byte = static_cast<unsigned char>(c);
        if (byte >= 0x20 && byte <= 0x7E && c != '%')
        {
            out.push_back(c);
        }
        else
        {
            out.push_back('%');
            out.push_back(kHexUpper[byte >> 4]);
            out.push_back(kHexUpper[byte & 0x0F]);
        }
    }
    return out;
}

std::string PercentDecodeGrpcMessage(std::string_view encoded)
{
    std::string out;
    out.reserve(encoded.size());
    for (size_t i = 0; i < encoded.size(); ++i)
    {
        if (encoded[i] == '%' && i + 2 < encoded.size())
        {
            const int high = HexDigitValue(encoded[i + 1]);
            const int low = HexDigitValue(encoded[i + 2]);
            if (high >= 0 && low >= 0)
            {
                out.push_back(static_cast<char>((high << 4) | low));
                i += 2;
                continue;
            }
        }
        out.push_back(encoded[i]);
    }
    return out;
}

} // namespace muduo_grpc
