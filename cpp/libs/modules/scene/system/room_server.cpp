#include "room_server.h"
#include <proto/common/common.pb.h>
#include <muduo/base/Logging.h>
#include "room_system.h"
#include "room_common.h"
#include <node_context_manager.h>
#include <scene/comp/node_scene_comp.h>
#include <services/centre/node/comp/game_node_comp.h>
#include <proto/logic/event/scene_event.pb.h>
#include <dispatcher_manager.h>

entt::entity RoomServer::CreateRoomOnRoomNode(const CreateRoomOnNodeRoomParam& param) {
	if (!param.CheckValid()) {
		LOG_ERROR << "Invalid parameters for creating room";
		return entt::null;
	}

	RoomInfoPBComponent roomInfo(param.roomInfo);
	if (roomInfo.guid() <= 0) {
		roomInfo.set_guid(RoomCommon::GenRoomGuid());
	}

	const auto room = TryCreateEntity(tlsRegistryManager.roomRegistry, entt::entity{ roomInfo.guid() });
	if (room == entt::null) {
		LOG_ERROR << "Failed to create room in registry";
		return entt::null;
	}

	tlsRegistryManager.roomRegistry.emplace<RoomInfoPBComponent>(room, std::move(roomInfo));
	tlsRegistryManager.roomRegistry.emplace<RoomPlayers>(room);

	auto& registry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);
	auto* serverPlayerInfo = registry.try_get<GameNodePlayerInfoPtrPBComponent>(param.node);
	if (serverPlayerInfo) {
		tlsRegistryManager.roomRegistry.emplace<GameNodePlayerInfoPtrPBComponent>(room, *serverPlayerInfo);
	}

	auto* pServerComp = registry.try_get<NodeNodeComp>(param.node);
	if (pServerComp) {
		pServerComp->AddScene(room);
	}

	OnRoomCreated createRoomEvent;
	createRoomEvent.set_entity(entt::to_integral(room));
	dispatcher.trigger(createRoomEvent);

	LOG_INFO << "Created new room with ID: " << tlsRegistryManager.roomRegistry.get<RoomInfoPBComponent>(room).guid();
	return room;
}

