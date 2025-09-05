#pragma once

#include "entt/src/entt/entity/entity.hpp"

struct CreateRoomOnNodeRoomParam;
struct DestroyRoomParam;

class RoomServer {
public:
	// 创建房间（只负责本地）
	static entt::entity CreateRoomOnRoomNode(const CreateRoomOnNodeRoomParam& param);

};
