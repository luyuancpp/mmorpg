#pragma once


// RoomServer.h
class RoomServer {
public:
	// �������䣨ֻ���𱾵أ�
	static entt::entity CreateRoomOnRoomNode(const CreateRoomOnNodeRoomParam& param);

	// ���ٷ��䣨ֻ���𱾵أ�
	static void DestroyRoom(const DestroyRoomParam& param);
};
