#pragma once

#include "util/node_id_generator.h"

using TransientNode32BitCompositeIdGenerator = TransientNodeCompositeIdGenerator<uint64_t, 32>;

class ThreadLocalIdGeneratorManager {
public:
    ThreadLocalIdGeneratorManager() = default;

    // 禁止拷贝和移动，确保单例唯一性
    ThreadLocalIdGeneratorManager(const ThreadLocalIdGeneratorManager&) = delete;
    ThreadLocalIdGeneratorManager& operator=(const ThreadLocalIdGeneratorManager&) = delete;

    /**
     * 获取线程局部的单例实例
     * @return 线程唯一的 ThreadLocalNodeContext 实例
     */
    static ThreadLocalIdGeneratorManager& Instance() {
        thread_local ThreadLocalIdGeneratorManager instance;
        return instance;
    }

	TransientNode32BitCompositeIdGenerator  buffIdGenerator;
	TransientNode32BitCompositeIdGenerator  skillIdGenerator;
};