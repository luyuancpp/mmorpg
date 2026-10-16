#pragma once

// 环境变量 GATE_ROUTER_TRANSPORT 的解析(docs/design/grpc-muduo-transport.md §8)。
//
// 第 3 批(§14.1)起开关失效:官方 gRPC 客户端已删除,gate → 路由服只走 muduo_h2。解析器只为兼容旧部署保留,
// 调用点(gate main.cpp)对显式设成 grpc 或不认识的值打 WARN 后照旧走 muduo_h2。下面是开关当初的语义:
//
//   * grpc(当初的默认):官方 gRPC C++ 运行时;
//   * muduo_h2:muduo 驱动的 HTTP/2 + 自有 gRPC 语义(cpp/libs/engine/muduo_grpc)。线上协议不变。
//
// 它只决定**传输**,与 GATE_CLIENT_RPC_ROUTER(业务拓扑:直连业务服务还是经路由服)是两回事:
// 直连模式下 gate 根本不连路由服,本开关不生效。启动时解析一次;改它要滚动重启 gate。
// 一次调用只走一种实现、只发一次;新实现失败**绝不**改走旧实现重发(服务端可能已经执行)。
//
// 与 gate_router_mode.h 同一条纪律:这里全是纯函数,不依赖 muduo / protobuf / 引擎,能单独编单测;日志由调用点打。

#include "security/token_security.h"

#include <cstdlib>
#include <string>
#include <string_view>

namespace gate_router_transport
{

inline constexpr char kTransportEnv[] = "GATE_ROUTER_TRANSPORT";

enum class Transport
{
    kOfficialGrpc,
    kMuduoH2,
};

struct Resolution
{
    Transport transport{Transport::kOfficialGrpc};
    // false = 设了一个不认识的值:解析结果按默认 grpc 填,调用点必须打 WARN。第 3 批起调用点不论解析结果都走 muduo_h2。
    bool recognized{true};
    std::string raw;
};

// 纯函数:忽略大小写与首尾空白;空串 = 未设置 = 默认。
inline Resolution ParseTransport(std::string_view raw)
{
    Resolution resolution;
    resolution.raw = std::string(raw);
    const std::string value = token_security::ToLowerAscii(token_security::TrimAscii(raw));
    if (value.empty() || value == "grpc")
    {
        return resolution;
    }
    if (value == "muduo_h2")
    {
        resolution.transport = Transport::kMuduoH2;
        return resolution;
    }
    resolution.recognized = false;
    return resolution;
}

inline Resolution ResolveTransportFromEnv(const char *envName)
{
    const char *raw = envName != nullptr ? std::getenv(envName) : nullptr;
    return ParseTransport(raw != nullptr ? std::string_view(raw) : std::string_view{});
}

// 进程内解析一次并缓存:每条经路由服的消息都会读到它,不能每次 getenv。
inline const Resolution &ResolveOnce()
{
    static const Resolution resolution = ResolveTransportFromEnv(kTransportEnv);
    return resolution;
}

inline const char *TransportName(Transport transport)
{
    return transport == Transport::kMuduoH2 ? "muduo_h2" : "grpc";
}

} // namespace gate_router_transport
