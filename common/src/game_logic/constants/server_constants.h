#pragma once

enum class ServerState : uint8_t
{
	kNormal,//game server 正常状态
	kMainTain,//game server 维护状态
	kCrash,//崩溃状态
};

enum class ServerPressureState : uint8_t
{
	kNoPressure,//无压力
	kPressure,//有压力
};

enum class ServerSceneType : uint8_t
{
	kMainSceneServer,//本服大世界场景
	kCrossMainSceneServer,//跨服大世界
	kRoomSceneServer,//本服普通房间场景
	kCrossRoomSceneServer,//跨服房间场景
};