#include "session_system.h"

#include "type_alias/session_id_gen.h"
#include "proto/common/session.pb.h"
#include <muduo/base/Logging.h>

NodeId GetGateNodeId(Guid session_id)
{
    return static_cast<NodeId>(session_id >> SessionIdGenerator::node_bit());
}

SessionDetailsPtr GetSessionDetailsByClientContext(const grpc::ClientContext& context)
{
    auto& initialMetadata = context.GetServerTrailingMetadata();
	for (const auto& pair : initialMetadata) {
		if (pair.first == "x-session-detail-bin")
		{
			auto sessionDetails = std::make_unique<SessionDetails>();
			if (sessionDetails->ParseFromString(pair.second.data()))
			{
				return sessionDetails;
			}
			else
			{
				LOG_ERROR << "Failed to parse session details from initial metadata.";
				return nullptr;
			}
		}
		else
		{
			LOG_DEBUG << "Skipping metadata key: " << pair.first.data();
		}
	}
	return nullptr;
}


