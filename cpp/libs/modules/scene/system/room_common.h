#pragma once
#include <core/type_define/type_define.h>

struct EnterRoomParam;
struct LeaveRoomParam;

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
};
