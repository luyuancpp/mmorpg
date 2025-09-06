#include "message_system.h"
#include "muduo/base/Logging.h"
#include "network/rpc_client.h"
#include "network/rpc_session.h"
#include "rpc/service_metadata/centre_service_service_metadata.h"
#include "rpc/service_metadata/gate_service_service_metadata.h"
#include "engine/session/comp/session_comp.h"
#include "engine/session/system/session.h"
#include "proto/logic/component/player_network_comp.pb.h"
#include "proto/common/node.pb.h"
#include "rpc/service_metadata/service_metadata.h"
#include "network/network_utils.h"
#include <engine/threading/registry_manager.h>
#include "engine/threading/node_context_manager.h"
#include <engine/threading/player_manager.h>


void SendToCentrePlayerById(uint32_t messageId, const google::protobuf::Message& message, Guid playerId)
{
	SendToCentrePlayerById(messageId, message, GetPlayer(playerId));
}

void SendToCentrePlayerById(uint32_t messageId, const google::protobuf::Message& message, entt::entity playerEntity)
{
	if (!tlsRegistryManager.actorRegistry.valid(playerEntity)){
		LOG_ERROR << "Player entity is not valid";
		return;
	}

	const auto* playerSessionSnapshotPB = tlsRegistryManager.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(playerEntity);
	if (!playerSessionSnapshotPB){
		LOG_ERROR << "Player node info not found for player entity";
		return;
	}

	auto nodeIt = playerSessionSnapshotPB->node_id().find(eNodeType::CentreNodeService);
	if (nodeIt == playerSessionSnapshotPB->node_id().end())
	{
		LOG_ERROR << "Centre node ID not found in player session snapshot";
		return;
	}

	entt::entity centreNodeId{ nodeIt->second };
	auto& centreNodeRegistry = tlsNodeContextManager.GetRegistry(eNodeType::CentreNodeService);
	if (!centreNodeRegistry.valid(centreNodeId)){
		LOG_ERROR << "Central node not found for player";
		return;
	}

	const auto node = centreNodeRegistry.try_get<RpcClientPtr>(centreNodeId);
	if (!node){
		LOG_ERROR << "RpcClientPtr not found for central node";
		return;
	}

	NodeRouteMessageRequest request;
	request.mutable_message_content()->set_message_id(messageId);
	request.mutable_message_content()->set_serialized_message(message.SerializeAsString());
	request.mutable_header()->set_session_id(playerSessionSnapshotPB->gate_session_id());
	(*node)->SendRequest(CentrePlayerServiceMessageId, request);
}


