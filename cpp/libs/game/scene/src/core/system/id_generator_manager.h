#pragma once

#include "util/node_id_generator.h"

using TransientNode32BitCompositeIdGenerator = TransientNodeCompositeIdGenerator<uint64_t, 32>;

class ThreadLocalIdGeneratorManager {
public:
    ThreadLocalIdGeneratorManager() = default;

    // 禁止拷贝和移动，确保单例唯一性
    ThreadLocalIdGeneratorManager(const ThreadLocalIdGeneratorManager&) = delete;
    ThreadLocalIdGeneratorManager& operator=(const ThreadLocalIdGeneratorManager&) = delete;

    void SetNodeId(uint32_t nodeId) {
        buffIdGenerator.set_node_id(nodeId);
        skillIdGenerator.set_node_id(nodeId);
    }

	TransientNode32BitCompositeIdGenerator  buffIdGenerator;
	TransientNode32BitCompositeIdGenerator  skillIdGenerator;
};

extern thread_local ThreadLocalIdGeneratorManager tlsIdGeneratorManager;
