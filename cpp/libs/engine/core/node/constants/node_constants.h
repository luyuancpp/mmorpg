#pragma once

#include <cstdint>

inline static constexpr std::size_t kMaxServerPlayerSize = 2000;
inline static constexpr std::size_t kMaxPlayersPerScene = 1000;

enum class NodeState : uint8_t
{
	kNormal,
	kMaintain,
	kCrash
};

enum class NodePressureState : uint8_t
{
	kNoPressure,
	kPressure
};

enum class ServerSceneType : uint8_t
{
	// local main-world scene
	kMainSceneServer,
	// cross-server main-world scene
	kCrossMainSceneServer,
	// local instanced scene
	kSceneServer,
	// cross-server instanced scene
	kCrossSceneServer
};