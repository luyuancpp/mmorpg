
#pragma once

#include "engine/core/type_define/type_define.h"
#include "engine/core/node/constants/node_constants.h"

#include "proto/common/component/game_node_comp.pb.h"

using ScenePlayers = EntityUnorderedSet; // weak refs; decouples players from scenes

struct MainSceneNode
{
};

struct CrossMainSceneNode
{
};

struct SceneSceneNode
{
};

struct CrossSceneSceneNode
{
};

class SceneRegistryComp {
public:
	// type alias
	using SceneList = std::unordered_map<uint32_t, EntityUnorderedSet>;

	[[nodiscard]] const SceneList& GetSceneMap() const { return scenesByConfigId; }

	std::size_t GetTotalSceneCount() const {
		std::size_t total = 0;
		for (const auto& [_, sceneSet] : scenesByConfigId)
			total += sceneSet.size();
		return total;
	}

	[[nodiscard]] const EntityUnorderedSet& GetScenesByConfig(uint32_t configId) const {
		static const EntityUnorderedSet emptySet;
		auto it = scenesByConfigId.find(configId);
		return it != scenesByConfigId.end() ? it->second : emptySet;
	}

private:
	SceneList scenesByConfigId;
};


struct NodeStateComp {
	// implicit conversion to enum for direct comparison
	operator NodeState() const {
		return state;
	}

	bool IsNormal() const {
		return state == NodeState::kNormal;
	}

	NodeState state = NodeState::kNormal;
};

struct NodePressureComp {
	operator NodePressureState() const {
		return state;
	}

	// convenience comparison operators
	bool operator==(NodePressureState other) const {
		return state == other;
	}
	bool operator!=(NodePressureState other) const {
		return state != other;
	}

	bool IsPressure() const {
		return state == NodePressureState::kPressure;
	}

	NodePressureState state = NodePressureState::kNoPressure;
};


using SceneNodePlayerStatsPtrComp = std::shared_ptr<GameNodePlayerInfoComp>;
