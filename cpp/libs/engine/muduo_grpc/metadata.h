#pragma once

// gRPC metadata(HTTP/2 头部里的自定义键值)与相关编码(docs/design/grpc-muduo-transport.md §4.2)。
//
// 本层只负责**线上**那一层编码:键以 "-bin" 结尾的值在线上是 base64。调用方给的、拿到的都是「应用层值」
// —— 例如 gate 的 x-session-detail-bin,应用层值本身就是一段 base64 文本(旧生成客户端先编了一层),
// 本层再按 -bin 规则编一层;接收时只解掉线上这一层。不要在这里「合并」两层。

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace muduo_grpc
{

// 有序多值表:同一个键可以出现多次,顺序与线上一致。键一律小写。
using Metadata = std::vector<std::pair<std::string, std::string>>;

// 在 metadata 里找第一个键等于 key 的值;没有返回 nullptr。
const std::string *FindMetadata(const Metadata &metadata, std::string_view key);

// 键以 "-bin" 结尾 = 二进制头,线上值是 base64。
bool IsBinaryHeaderKey(std::string_view key);

// 调用方能否把这个键作为自定义 metadata 发出:非空、只含 [0-9a-z_.-]、不以 ':' 或 "grpc-" 开头、
// 不是 HTTP/2 禁止的连接级头,也不是本层自己负责的保留头(te / content-type / user-agent)。
bool IsValidCustomMetadataKey(std::string_view key);

// 非二进制头的值必须是可打印 ASCII(0x20..0x7E)。
bool IsValidAsciiMetadataValue(std::string_view value);

// RFC 4648 §4 标准字母表。发送按规范建议不带 '=' padding(与 grpc C++ / grpc-go 发送一致)。
std::string Base64EncodeUnpadded(std::string_view input);

// 接收方向:有无 padding 都接受;遇到非法字符或非法长度返回 nullopt。不接受 URL 安全字母表。
std::optional<std::string> Base64DecodeLenient(std::string_view input);

// 二进制头的线上值 → 应用层值。规范要求:同名头被合并成逗号分隔时先按逗号拆开逐段解码,再拼接。
std::optional<std::string> DecodeBinaryHeaderValue(std::string_view wireValue);

// grpc-message 的百分号编码:空格与 0x20..0x7E 中除 '%' 外的字符原样,其余字节写成 %XX(大写十六进制)。
std::string PercentEncodeGrpcMessage(std::string_view message);

// 解码永不失败:非法的 % 序列原样保留(规范要求不得因解码失败丢弃消息)。
std::string PercentDecodeGrpcMessage(std::string_view encoded);

} // namespace muduo_grpc
