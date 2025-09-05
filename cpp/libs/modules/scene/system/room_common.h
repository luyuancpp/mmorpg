#pragma once
#include <core/type_define/type_define.h>

struct EnterRoomParam
{
	inline bool CheckValid() const
	{
		return room != entt::null && enter != entt::null;
	}

	entt::entity room{ entt::null };
	entt::entity enter{ entt::null };
};

struct LeaveRoomParam
{
	inline bool CheckValid() const
	{
		return leaver != entt::null;
	}

	entt::entity leaver{ entt::null };
};

struct DestroyRoomParam
{
	inline bool CheckValid() const
	{
		return node != entt::null && room != entt::null;
	}

	entt::entity node{ entt::null };
	entt::entity room{ entt::null };
};

struct CreateRoomOnNodeRoomParam;

// RoomCommon.h
class RoomCommon {
public:
	static NodeId GetGameNodeIdFromGuid(uint64_t room_id);
	static entt::entity GetRoomNodeEntityId(uint64_t room_id);
	static void SetSequenceNodeId(uint32_t node_id);
	static void ClearAllRoomData();

	static uint32_t GenRoomGuid();

	static bool IsRoomEmpty();
	static std::size_t GetRoomsSize();
	static std::size_t GetRoomsSize(uint32_t roomConfigId);
	static bool ConfigRoomListNotEmpty(uint32_t roomConfigId);

	static uint32_t CheckPlayerEnterRoom(const EnterRoomParam& param);
	static uint32_t HasRoomSlot(entt::entity room);

	static void EnterRoom(const EnterRoomParam& param);
	static void LeaveRoom(const LeaveRoomParam& param);

	// 销毁房间（只负责本地）
	static void DestroyRoom(const DestroyRoomParam& param);

	static entt::entity CreateRoomOnRoomNode(const CreateRoomOnNodeRoomParam& param);

};
