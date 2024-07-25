#pragma once

#include "constants/node.h"

#include "type_define/type_define.h"

constexpr uint32_t kDefaultSceneId = 1;

struct GetSceneParam
{
	uint32_t sceneConfId{kDefaultSceneId};
};

struct GetSceneFilterParam
{
	NodePressureState nodePressureState{NodePressureState::kNoPressure};
};

struct CompelChangeSceneParam
{
	inline bool IsNull() const
	{
		return player == entt::null || destNode == entt::null;
	}

	entt::entity player{entt::null};
	entt::entity destNode{entt::null};
	uint32_t sceneConfId{0};
};

class NodeSceneSystem
{
public:
    //从人数少的服务器中找到一个对应场景人数最少的,效率有些低
	static entt::entity FindSceneWithMinPlayerCount(const GetSceneParam& param);

	//得到有该场景的人数不满的服务器,效率比上面函数高一点点
	static entt::entity FindNotFullScene(const GetSceneParam& param);

	static void SetNodePressure(entt::entity node);

	static void ClearNodePressure(entt::entity node);

	static void SetNodeState(entt::entity node, NodeState node_state);
};

