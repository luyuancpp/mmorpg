#pragma once

#include "util/game_registry.h"

#include "proto/common/node.pb.h"

#include <type_define/type_define.h>

using NodeRgistries = std::array<entt::registry, eNodeType_ARRAYSIZE>;
using NodeGloabalEntity = std::array<entt::entity, eNodeType_ARRAYSIZE>;

class ThreadLocalNodeContext {
public:
	ThreadLocalNodeContext() = default;

	static ThreadLocalNodeContext& Instance() {
		thread_local ThreadLocalNodeContext instance;
		return instance;
	}

	// 获取指定类型的 registry
	entt::registry& GetRegistry(uint32_t nodeType) {
		return registries_[nodeType];
	}

	// 获取所有 registry（引用）
	NodeRgistries& GetAllRegistries() {
		return registries_;
	}

	// 设置全局 entity
	void SetGlobalEntity(uint32_t nodeType, entt::entity entity) {
		globalEntities[nodeType] = entity;
	}

	// 获取全局 entity
	entt::entity GetGlobalEntity(uint32_t nodeType) const {
		return globalEntities[nodeType];
	}

	// 清理所有数据
	void Clear() {
		for (auto& registry : registries_) {
			registry.clear();
		}
		globalEntities.fill(entt::null);
	}

	entt::entity GetNodeGlobalEntity(uint32_t nodeType)
	{
		auto& registry = GetRegistry(nodeType);
		if (globalEntities[nodeType] == entt::null)
		{
			globalEntities[nodeType] = registry.create();
		}
		return globalEntities[nodeType];
	}

private:
	NodeRgistries registries_;
	NodeGloabalEntity globalEntities;
};
