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