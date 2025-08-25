#include "message_system.h"
#include "muduo/base/Logging.h"
#include "network/rpc_client.h"
#include "network/rpc_session.h"
#include "service_info/centre_service_service_info.h"
#include "service_info/gate_service_service_info.h"
#include "threading/storage.h"
#include "threading/storage_common_logic.h"
#include "game_common_logic/comp/session_comp.h"
#include "game_common_logic/system/session_system.h"
#include "proto/logic/component/player_network_comp.pb.h"
#include "proto/common/node.pb.h"
#include "service_info/service_info.h"
#include "util/network_utils.h"
#include "node/system/node_system.h"


void SendToCentrePlayerById(uint32_t messageId, const google::protobuf::Message& message, Guid playerId)
{
	SendToCentrePlayerById(messageId, message, tlsCommonLogic.GetPlayer(playerId));
}

void SendToCentrePlayerById(uint32_t messageId, const google::protobuf::Message& message, entt::entity playerEntity)
{
	if (!tls.actorRegistry.valid(playerEntity)){
		LOG_ERROR << "Player entity is not valid";
		return;
	}

	const auto* playerSessionSnapshotPB = tls.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(playerEntity);
	if (!playerSessionSnapshotPB){
		LOG_ERROR << "Player node info not found for player entity";
		return;
	}

	entt::entity centreNodeId{ playerSessionSnapshotPB->centre_node_id() };
	auto& centreNodeRegistry = tls.GetNodeRegistry(eNodeType::CentreNodeService);
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


