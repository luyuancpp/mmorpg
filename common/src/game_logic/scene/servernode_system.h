#pragma once

#include "src/game_logic/constants/server_constants.h"

#include "src/common_type/common_type.h"

struct GetSceneParam
{
	uint32_t scene_conf_id_{0};
};

struct GetSceneFilterParam
{
	NodePressureState server_pressure_state_{NodePressureState::kNoPressure};
};

struct ServerStateParam
{
	inline bool IsNull() const
	{
		return node_entity_ == entt::null;
	}

	entt::entity node_entity_{entt::null};
	NodeState node_state_{NodeState::kNormal};
};

struct ReplaceCrashServerParam
{
	inline bool IsNull() const
	{
		return cransh_node_ == entt::null || replace_node_ == entt::null;
	}

	entt::entity cransh_node_{entt::null};
	entt::entity replace_node_{entt::null};
};

struct CompelChangeSceneParam
{
	inline bool IsNull() const
	{
		return player_ == entt::null || dest_node_ == entt::null;
	}

	entt::entity player_{entt::null};
	entt::entity dest_node_{entt::null};
	uint32_t scene_conf_id_{0};
};

class ServerNodeSystem
{
public:
	//得到有该场景的人数最少的服务器
	static entt::entity GetSceneOnMinPlayerSizeNode(const GetSceneParam& param);

	//得到有该场景的人数不满的服务器
	static entt::entity GetNotFullScene(const GetSceneParam& param);

	static void ServerEnterPressure(entt::entity node);

	static void ServerEnterNoPressure(entt::entity node);

	static void SetServerState(entt::entity node, NodeState node_state);

};

