#pragma once

#include <memory>

#include "redis_client/redis_client.h"

class RedisSystem
{
public:
    RedisSystem() = default;
    // 禁止拷贝和移动，确保单例唯一性
    RedisSystem(const RedisSystem&) = delete;
    RedisSystem& operator=(const RedisSystem&) = delete;

    /**
     * 获取线程局部的单例实例
     * @return 线程唯一的 ThreadLocalNodeContext 实例
     */
    static RedisSystem& Instance() {
        thread_local RedisSystem instance;
        return instance;
    }

    void Initialize();
private:
};


