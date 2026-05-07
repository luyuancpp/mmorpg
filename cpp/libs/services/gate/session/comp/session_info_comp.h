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
	//
	// Naming note: these methods are named Entity*-Id (not Node*-Id) because
	// what we track per nodeType is the local entt::entity integer for the
	// remote node — NOT the business `node_id` on that node. Post uuid-primary-
	// key refactor of node_connector, entity and node_id are no longer equal,
	// so using "EntityId" terminology prevents a common footgun where callers
	// would assume the returned value can be fed into `entt::entity{...}` and
	// land on a valid slot without going through a FindNodeEntity* helper.
	static constexpr uint64_t kInvalidEntityId{UINT64_MAX};
	using NodeEntityMap = std::unordered_map<uint32_t, uint64_t>;

	SessionInfo() = default;

	void SetEntityId(uint32_t nodeType, uint64_t entityId)
	{
		entityIds[nodeType] = entityId;
	}

	uint64_t GetEntityId(uint32_t nodeType) const
	{
		auto it = entityIds.find(nodeType);
		if (it != entityIds.end())
		{
			return it->second;
		}
		return kInvalidEntityId;
	}

	bool HasEntityId(uint32_t nodeType) const
	{
		auto it = entityIds.find(nodeType);
		if (it == entityIds.end())
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
	NodeEntityMap entityIds; // Sparse map, only stores assigned node entities
};
