#pragma once

#include "core/utils/id/snow_flake.h"
#include <engine/core/type_define/type_define.h>

class SnowFlakeManager
{
public:

	void OnNodeStart(uint32_t nodeId) {
		itemIdGenerator_.set_node_id(nodeId);
	}

	void SetGuardTime(uint64_t guardUtcSeconds) {
		itemIdGenerator_.SetGuardTime(guardUtcSeconds);
	}

	Guid GenerateItemGuid() {
		lastGeneratedItemGuid_ = itemIdGenerator_.Generate();
		return lastGeneratedItemGuid_;
	}

	[[nodiscard]] Guid GetLastGeneratedItemGuid() const { return lastGeneratedItemGuid_; }

private:
	Guid lastGeneratedItemGuid_{ kInvalidGuid };
	SnowFlake itemIdGenerator_;
};

extern thread_local SnowFlakeManager tlsSnowflakeManager;