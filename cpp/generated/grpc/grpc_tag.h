#pragma once
#include <cstdint>

struct GrpcTag {
	uint32_t messageId;
	void* valuePtr;
};
