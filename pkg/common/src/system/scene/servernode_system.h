#pragma once

#include "src/constants/server_constants.h"

#include "src/type_define/type_define.h"

constexpr uint32_t kDefaultSceneId = 1;

struct GetSceneParam
{
	uint32_t scene_conf_id_{kDefaultSceneId};
};

struct GetSceneFilterParam
{
	NodePressureState node_pressure_state_{NodePressureState::kNoPressure};
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

	//得到有该场景的人数不满的服务器,效率比上面函数高一点点
	static entt::entity GetNotFullScene(const GetSceneParam& param);

	static void NodeEnterPressure(entt::entity node);

	static void NodeEnterNoPressure(entt::entity node);

	static void SetNodeState(entt::entity node, NodeState node_state);
};

