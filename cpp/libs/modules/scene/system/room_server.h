#pragma once


// RoomServer.h
class RoomServer {
public:
	// 创建房间（只负责本地）
	static entt::entity CreateRoomOnRoomNode(const CreateRoomOnNodeRoomParam& param);

	// 销毁房间（只负责本地）
	static void DestroyRoom(const DestroyRoomParam& param);
};
