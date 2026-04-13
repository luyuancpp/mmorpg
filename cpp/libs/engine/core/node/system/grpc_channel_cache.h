#pragma once

#include <grpcpp/grpcpp.h>
#include <grpcpp/resource_quota.h>

#include <cstdlib>
#include <memory>
#include <string>
#include <unordered_map>

namespace grpc_channel_cache {

inline int GetGrpcMaxThreads() {
    const char* env = std::getenv("GRPC_MAX_THREADS");
    if (env == nullptr || env[0] == '\0') {
        return 2;
    }
    const int parsed = std::atoi(env);
    return parsed > 0 ? parsed : 2;
}

inline int GetGrpcBackupPollIntervalMs() {
    const char* env = std::getenv("GRPC_CLIENT_CHANNEL_BACKUP_POLL_INTERVAL_MS");
    if (env == nullptr || env[0] == '\0') {
        return 5000;
    }
    const int parsed = std::atoi(env);
    return parsed >= 0 ? parsed : 5000;
}

inline int GetGrpcThreadPoolReserveThreads() {
    const char* env = std::getenv("GRPC_THREAD_POOL_RESERVE_THREADS");
    if (env == nullptr || env[0] == '\0') {
        return 0;
    }
    const int parsed = std::atoi(env);
    return parsed > 0 ? parsed : 0;
}

inline int GetGrpcThreadPoolMaxThreads() {
    const char* env = std::getenv("GRPC_THREAD_POOL_MAX_THREADS");
    if (env == nullptr || env[0] == '\0') {
        return 0;
    }
    const int parsed = std::atoi(env);
    return parsed > 0 ? parsed : 0;
}

inline int ConfiguredMaxThreads() {
    return GetGrpcMaxThreads();
}

inline int ConfiguredBackupPollIntervalMs() {
    return GetGrpcBackupPollIntervalMs();
}

inline int ConfiguredThreadPoolReserveThreads() {
    return GetGrpcThreadPoolReserveThreads();
}

inline int ConfiguredThreadPoolMaxThreads() {
    return GetGrpcThreadPoolMaxThreads();
}

class GrpcChannelCache {
public:
    GrpcChannelCache()
        : resourceQuota_("node-grpc-client-rq"),
          maxThreads_(GetGrpcMaxThreads()),
          backupPollIntervalMs_(GetGrpcBackupPollIntervalMs()),
          reserveThreads_(GetGrpcThreadPoolReserveThreads()),
          maxThreadPoolThreads_(GetGrpcThreadPoolMaxThreads()) {
        resourceQuota_.SetMaxThreads(maxThreads_);
    }

    int ConfiguredMaxThreads() const {
        return maxThreads_;
    }

    int ConfiguredBackupPollIntervalMs() const {
        return backupPollIntervalMs_;
    }

    int ConfiguredThreadPoolReserveThreads() const {
        return reserveThreads_;
    }

    int ConfiguredThreadPoolMaxThreads() const {
        return maxThreadPoolThreads_;
    }

    size_t CachedTargetCount() {
        for (auto it = channelCache_.begin(); it != channelCache_.end();) {
            if (it->second.expired()) {
                it = channelCache_.erase(it);
            } else {
                ++it;
            }
        }
        return channelCache_.size();
    }

    std::shared_ptr<grpc::Channel> GetOrCreateChannel(const std::string& target) {
        auto it = channelCache_.find(target);
        if (it != channelCache_.end()) {
            if (auto existing = it->second.lock()) {
                return existing;
            }
        }

        grpc::ChannelArguments args;
        args.SetResourceQuota(resourceQuota_);
        args.SetInt("grpc.client_channel_backup_poll_interval_ms", backupPollIntervalMs_);
        auto channel = grpc::CreateCustomChannel(target, grpc::InsecureChannelCredentials(), args);
        channelCache_[target] = channel;
        return channel;
    }

private:
    std::unordered_map<std::string, std::weak_ptr<grpc::Channel>> channelCache_;
    grpc::ResourceQuota resourceQuota_;
    int maxThreads_;
    int backupPollIntervalMs_;
    int reserveThreads_;
    int maxThreadPoolThreads_;
};

}  // namespace grpc_channel_cache
