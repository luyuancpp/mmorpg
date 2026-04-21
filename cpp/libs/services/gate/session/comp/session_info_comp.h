#pragma once

#include <unordered_map>

#include "muduo/net/TcpConnection.h"
#include "engine/core/type_define/type_define.h"
#include "message_limiter/message_limiter.h"

struct SessionInfo
{
	// Entity IDs are uint64_t (ENTT_ID_TYPE=uint64_t with version bits).
	// Using uint64_t avoids truncation that loses entity version/generation,
	// which causes registry.valid() to reject recycled entities.
	static constexpr uint64_t kInvalidEntityId{UINT64_MAX};
	using NodeEntityMap = std::unordered_map<uint32_t, uint64_t>;

	SessionInfo() = default;

	void SetNodeId(uint32_t nodeType, uint64_t entityId)
	{
		nodeIds[nodeType] = entityId;
	}

	uint64_t GetNodeId(uint32_t nodeType) const
	{
		auto it = nodeIds.find(nodeType);
		if (it != nodeIds.end())
		{
			return it->second;
		}
		return kInvalidEntityId;
	}

	bool HasNodeId(uint32_t nodeType) const
	{
		auto it = nodeIds.find(nodeType);
		if (it == nodeIds.end())
		{
			return false;
		}
		return it->second != kInvalidEntityId;
	}

	Guid playerId{kInvalidGuid};
	muduo::net::TcpConnectionPtr conn;
	MessageLimiter messageLimiter;
	uint32_t sessionVersion{UINT32_MAX};
	uint32_t pendingEnterGsType{0}; // Pending login type to forward to Scene once scene node is assigned
	uint64_t sceneId{0};            // Scene instance GUID from SceneManager (RoutePlayerEvent)
	bool verified{false};			// True after client passes Gate connection token verification
private:
	NodeEntityMap nodeIds; // Sparse map, only stores assigned node entities
};
