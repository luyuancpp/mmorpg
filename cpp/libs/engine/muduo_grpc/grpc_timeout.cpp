#include "grpc_timeout.h"

#include <charconv>
#include <cstdint>
#include <limits>

namespace muduo_grpc
{

namespace
{

constexpr int64_t kMaxTimeoutValue = 99'999'999; // 8 位十进制

struct TimeoutUnit
{
    int64_t nanosPerUnit;
    char suffix;
};

// 由细到粗:与 grpc-go encodeDuration 的尝试顺序相同。
constexpr TimeoutUnit kUnits[] = {
    {1, 'n'},
    {1'000, 'u'},
    {1'000'000, 'm'},
    {1'000'000'000, 'S'},
    {60LL * 1'000'000'000, 'M'},
    {3600LL * 1'000'000'000, 'H'},
};

// 向上取整的整除(d > 0, r > 0)。
int64_t DivRoundUp(int64_t d, int64_t r)
{
    return d / r + (d % r > 0 ? 1 : 0);
}

} // namespace

std::string EncodeGrpcTimeout(std::chrono::nanoseconds timeout)
{
    const int64_t nanos = timeout.count();
    if (nanos <= 0)
    {
        return "0n";
    }
    for (const auto &unit : kUnits)
    {
        const int64_t value = DivRoundUp(nanos, unit.nanosPerUnit);
        if (value <= kMaxTimeoutValue || unit.suffix == 'H')
        {
            // 小时是最后一档:int64 纳秒最多约 2.56e6 小时,7 位,必然放得下。
            std::string text = std::to_string(value);
            text.push_back(unit.suffix);
            return text;
        }
    }
    return "0n"; // 不可达
}

std::optional<std::chrono::nanoseconds> ParseGrpcTimeout(std::string_view value)
{
    if (value.size() < 2 || value.size() > 9)
    {
        return std::nullopt;
    }
    const char suffix = value.back();
    const std::string_view digits = value.substr(0, value.size() - 1);
    for (const char c : digits)
    {
        if (c < '0' || c > '9')
        {
            return std::nullopt;
        }
    }
    int64_t amount = 0;
    const auto [ptr, ec] = std::from_chars(digits.data(), digits.data() + digits.size(), amount);
    if (ec != std::errc{} || ptr != digits.data() + digits.size())
    {
        return std::nullopt;
    }
    for (const auto &unit : kUnits)
    {
        if (unit.suffix == suffix)
        {
            // 8 位 × 3600e9 ≈ 3.6e17 < int64 上限,不会溢出。
            return std::chrono::nanoseconds(amount * unit.nanosPerUnit);
        }
    }
    return std::nullopt;
}

} // namespace muduo_grpc
