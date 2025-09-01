#pragma once

#include "core/utils/snow_flake.h"
#include <base/core/type_define/type_define.h>

class SnowFlakeManager
{
public:

	void OnNodeStart(uint32_t nodeId) {
		itemIdGenerator.set_node_id(nodeId);
	}

	Guid lastGeneratorItemGuid{ kInvalidGuid };
	SnowFlake itemIdGenerator;
};

extern thread_local SnowFlakeManager tlsSnowflakeManager;