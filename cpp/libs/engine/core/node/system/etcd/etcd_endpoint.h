#pragma once

// etcd 地址解析的纯函数部分(docs/design/grpc-muduo-transport.md §13.6),头文件内联以便单测直接包含。

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>

namespace etcd_transport
{
struct Endpoint
{
    std::string host;
    uint16_t port{0};
};

// 把 Etcd.Hosts 里的一项拆成主机与端口:"host:port",可带 "http://" 前缀与结尾的 "/"。
// https(本传输不做 TLS)、IPv6 字面量("[...]",muduo_grpc 只连 IPv4)、缺主机 / 缺端口、端口非数字或越界一律失败。
inline bool ParseEndpoint(std::string_view endpoint, Endpoint &out, std::string &error)
{
    constexpr std::string_view kHttp = "http://";
    constexpr std::string_view kHttps = "https://";
    if (endpoint.starts_with(kHttps))
    {
        error = "https endpoints are not supported (no TLS on the muduo_h2 etcd transport)";
        return false;
    }
    if (endpoint.starts_with(kHttp))
    {
        endpoint.remove_prefix(kHttp.size());
    }
    while (endpoint.ends_with('/'))
    {
        endpoint.remove_suffix(1);
    }
    if (endpoint.starts_with('['))
    {
        error = "IPv6 endpoints are not supported";
        return false;
    }
    const size_t colon = endpoint.rfind(':');
    if (colon == std::string_view::npos || colon == 0 || colon + 1 == endpoint.size())
    {
        error = "expected host:port";
        return false;
    }
    const std::string_view host = endpoint.substr(0, colon);
    const std::string_view portText = endpoint.substr(colon + 1);
    if (host.find(':') != std::string_view::npos || host.find('/') != std::string_view::npos)
    {
        error = "malformed host '" + std::string(host) + "'";
        return false;
    }
    unsigned value = 0;
    const auto [ptr, ec] = std::from_chars(portText.data(), portText.data() + portText.size(), value);
    if (ec != std::errc{} || ptr != portText.data() + portText.size() || value == 0 || value > 65535)
    {
        error = "invalid port '" + std::string(portText) + "'";
        return false;
    }
    out.host = std::string(host);
    out.port = static_cast<uint16_t>(value);
    return true;
}
} // namespace etcd_transport
