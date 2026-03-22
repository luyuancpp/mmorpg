#pragma once

#include "core/utils/id/node_id_generator.h"

using TransientNode32BitCompositeIdGenerator = TransientNodeCompositeIdGenerator<uint64_t, 32>;

class ThreadLocalIdGeneratorManager {
public:
    ThreadLocalIdGeneratorManager() = default;

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
