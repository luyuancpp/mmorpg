#include "thread_local/storage.h"

#include "proto/common/common.pb.h"

NodeInfo& GetNodeInfo()
{
	return tls.globalRegistry.get_or_emplace<NodeInfo>(GlobalEntity());
}
