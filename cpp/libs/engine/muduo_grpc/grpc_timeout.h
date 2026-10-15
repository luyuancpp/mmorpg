#pragma once

// grpc-timeout 请求头的编解码(PROTOCOL-HTTP2.md:TimeoutValue 最多 8 位十进制 + 单位 H/M/S/m/u/n)。
//
// 编码与 grpc-go internal/grpcutil/encode_duration.go 逐字节一致:从纳秒开始,取第一个能放进 8 位的单位,
// 向上取整;非正值编成 "0n"。例:800ms → "800000u",10s → "10000000u",100s → "100000m"。
// 超过 8 位的值 grpc-go 服务端会以 400 + Internal 拒绝整个请求,所以这里绝不产出。

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace muduo_grpc
{

std::string EncodeGrpcTimeout(std::chrono::nanoseconds timeout);

// 服务端方向(第 4 批)与单测用:格式非法返回 nullopt。
std::optional<std::chrono::nanoseconds> ParseGrpcTimeout(std::string_view value);

} // namespace muduo_grpc
