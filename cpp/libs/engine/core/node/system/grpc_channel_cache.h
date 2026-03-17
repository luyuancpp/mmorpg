#pragma once

#include <grpcpp/grpcpp.h>
#include <grpcpp/resource_quota.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <cstdlib>

namespace grpc_channel_cache {

inline int GetGrpcMaxThreads() {
    // Keep a conservative default. Can be overridden by env at process start.
    const char* env = std::getenv("GRPC_MAX_THREADS");
    if (env == nullptr || env[0] == '\0') {
        return 2;
    }
    const int parsed = std::atoi(env);
    return parsed > 0 ? parsed : 2;
}

inline int ConfiguredMaxThreads() {
    return GetGrpcMaxThreads();
}

inline int GetGrpcBackupPollIntervalMs() {
    // gRPC default is 5000ms. Use 0 to disable backup poll for testing.
    const char* env = std::getenv("GRPC_CLIENT_CHANNEL_BACKUP_POLL_INTERVAL_MS");
    if (env == nullptr || env[0] == '\0') {
        return 5000;
    }
    const int parsed = std::atoi(env);
    return parsed >= 0 ? parsed : 5000;
}

inline int ConfiguredBackupPollIntervalMs() {
    return GetGrpcBackupPollIntervalMs();
}

inline int GetGrpcThreadPoolReserveThreads() {
    const char* env = std::getenv("GRPC_THREAD_POOL_RESERVE_THREADS");
    if (env == nullptr || env[0] == '\0') {
        return 0;
    }
    const int parsed = std::atoi(env);
    return parsed > 0 ? parsed : 0;
}

inline int ConfiguredThreadPoolReserveThreads() {
    return GetGrpcThreadPoolReserveThreads();
}

inline int GetGrpcThreadPoolMaxThreads() {
    const char* env = std::getenv("GRPC_THREAD_POOL_MAX_THREADS");
    if (env == nullptr || env[0] == '\0') {
        return 0;
    }
    const int parsed = std::atoi(env);
    return parsed > 0 ? parsed : 0;
}

inline int ConfiguredThreadPoolMaxThreads() {
    return GetGrpcThreadPoolMaxThreads();
}

inline std::unordered_map<std::string, std::weak_ptr<grpc::Channel>>& ChannelCache() {
    static std::unordered_map<std::string, std::weak_ptr<grpc::Channel>> channelCache;
    return channelCache;
}

inline size_t CachedTargetCount() {
    // Accessed from a single thread in this runtime path.
    auto& channelCache = ChannelCache();
    for (auto it = channelCache.begin(); it != channelCache.end(); ) {
        if (it->second.expired()) {
            it = channelCache.erase(it);
        } else {
            ++it;
        }
    }
    return channelCache.size();
}

inline std::shared_ptr<grpc::Channel> GetOrCreateChannel(const std::string& target) {
    // Accessed from a single thread in this runtime path.
    // Keep one channel per target. A channel is bound to its endpoint.
    auto& channelCache = ChannelCache();
    static grpc::ResourceQuota resourceQuota("global-grpc-client-rq");
    static const int kMaxThreads = GetGrpcMaxThreads();
    static const int kBackupPollIntervalMs = GetGrpcBackupPollIntervalMs();
    static const bool quotaInitialized = (resourceQuota.SetMaxThreads(kMaxThreads), true);
    (void)quotaInitialized;

    auto it = channelCache.find(target);
    if (it != channelCache.end()) {
        if (auto existing = it->second.lock()) {
            return existing;
        }
    }

    grpc::ChannelArguments args;
    args.SetResourceQuota(resourceQuota);
    args.SetInt("grpc.client_channel_backup_poll_interval_ms", kBackupPollIntervalMs);
    auto channel = grpc::CreateCustomChannel(target, grpc::InsecureChannelCredentials(), args);
    channelCache[target] = channel;

    return channel;
}

}  // namespace grpc_channel_cache
