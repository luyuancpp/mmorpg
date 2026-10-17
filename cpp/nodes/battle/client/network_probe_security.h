#pragma once

#include <charconv>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <string_view>
#include "security/token_security.h"

namespace network_probe
{
inline constexpr std::string_view kGrantDomain{"xuanming.match.probe.grant.v1\0", sizeof("xuanming.match.probe.grant.v1\0") - 1};
inline constexpr std::string_view kReceiptDomain{"xuanming.match.probe.receipt.v1\0", sizeof("xuanming.match.probe.receipt.v1\0") - 1};
inline bool Enabled()
{
    const auto* raw=std::getenv("GLOBAL_MATCH_ENABLED");return raw&&std::string_view(raw)=="true";
}
inline uint32_t ParsePhysicalRegion(std::string_view text)
{
    uint32_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size() ? value : 0;
}
inline uint32_t PhysicalRegion(){const auto* raw=std::getenv("PHYSICAL_REGION");return raw?ParsePhysicalRegion(raw):0;}
inline const std::string& Secret()
{
    static const std::string value = [] {const auto* p = std::getenv("MATCH_PROBE_SECRET"); return p ? std::string(p) : std::string{};}();
    return value;
}
inline std::string SignWithSecret(std::string_view secret,std::string_view domain, std::string_view payload)
{
    if (secret.size() < 32) return {};
    std::string input(domain); input.append(payload);
    unsigned char digest[EVP_MAX_MD_SIZE]; unsigned int count = 0;
    if (!HMAC(EVP_sha256(), secret.data(), static_cast<int>(secret.size()),
        reinterpret_cast<const unsigned char*>(input.data()), input.size(), digest, &count)) return {};
    return {reinterpret_cast<const char*>(digest), count};
}
inline std::string Sign(std::string_view domain,std::string_view payload){return SignWithSecret(Secret(),domain,payload);}
}
