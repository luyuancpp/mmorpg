#include "proto_util.h"
#include <unordered_set>
#include "proto/common/node.pb.h"

bool IsTcpNodeType(uint32_t nodeType) {
	static const std::unordered_set<uint32_t> validTypes = {
	};

	return validTypes.contains(nodeType);
}