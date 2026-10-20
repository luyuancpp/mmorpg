#pragma once

// 地址字面量校验(docs/design/grpc-muduo-transport.md §15.3 Q8、§15.4.2)。
//
// 服务端 bind(server.cpp)与客户端拨号(channel.cpp)共用这一份规则(§15.4.2)。channel.cpp 以前的私有副本
// 用有符号的 from_chars 解析每段,会把 "-1.2.3.4"、"1.2.3.-0" 当成合法,已删除;本实现逐字符校验。

#include <string>

namespace muduo_grpc
{

// 只接受规范的点分十进制 IPv4:恰好 4 段,每段 1..3 位十进制、不超过 255、不带前导零;
// 不接受空段、主机名、IPv6、首尾空白。muduo 的 InetAddress 遇到非法 IP 只打一行日志并留下 0.0.0.0
// —— 服务端会因此监听全部网卡,客户端在 Linux 上会连到本机,所以必须在构造 InetAddress 之前校验。
bool IsCanonicalIpv4(const std::string &ip);

} // namespace muduo_grpc
