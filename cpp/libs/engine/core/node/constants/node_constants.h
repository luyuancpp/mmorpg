#pragma once

#include <cstdint>
#include <cstddef>

inline static constexpr std::size_t kMaxServerPlayerSize = 2000;
inline static constexpr std::size_t kMaxPlayersPerRoom = 1000;

enum class NodeState : uint8_t
{
	//game server 正常状态
	kNormal,
	//game server 维护状态
	kMaintain,
	//崩溃状态
	kCrash
};

enum class NodePressureState : uint8_t
{
	//无压力
	kNoPressure,
	//有压力
	kPressure
};

enum class ServerSceneType : uint8_t
{
	//本服大世界场景
	kMainSceneServer,
	//跨服大世界
	kCrossMainSceneServer,
	//本服普通房间场景
	kRoomSceneServer,
	//跨服房间场景
	kCrossRoomSceneServer
};