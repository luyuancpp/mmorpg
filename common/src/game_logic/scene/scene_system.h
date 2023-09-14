#pragma once

#include "src/game_logic/comp/server_comp.h"

#include "src/game_logic/scene/servernode_system.h"
#include "src/util/snow_flake.h"
#include <component_proto/scene_comp.pb.h>

struct EnterSceneParam
{
	inline bool IsNull() const
	{
		return scene_ == entt::null || player_ == entt::null;
	}

	entt::entity scene_{entt::null};
	entt::entity player_{entt::null};
};

struct EnterDefaultSceneParam
{
	inline bool IsNull() const
	{
		return player_ == entt::null;
	}

	entt::entity player_{entt::null};
};

struct LeaveSceneParam
{
	inline bool IsNull() const
	{
		return leaver_ == entt::null;
	}

	entt::entity leaver_{entt::null};
};

struct CreateGsSceneParam
{
	inline bool IsNull() const
	{
		return node_ == entt::null;
	}

	entt::entity node_{entt::null};
	SceneInfo scene_info;
	uint32_t scene_confid_{0};
};

void AddMainSceneNodeComponent(entt::entity server);


//todo 1线镜像和一线应该是同一个gs,这样就不会有切换服务器的开销
class ScenesSystem
{
public:
	//just for test clear
	ScenesSystem();
	~ScenesSystem();

	static std::size_t scenes_size(uint32_t scene_config_id);
	static std::size_t scenes_size();
	static void set_server_sequence_node_id(const uint32_t node_id) { server_sequence_.set_node_id(node_id); }

	static entt::entity GetSceneByGuid(Guid guid);

	static bool ConfigSceneListNotEmpty(uint32_t scene_config_id);
	static bool IsSceneEmpty();

	static uint32_t CheckScenePlayerSize(entt::entity scene);

	static entt::entity CreateScene2Gs(const CreateGsSceneParam& param);

	static void DestroyScene(entt::entity node, entt::entity scene);
	static void OnDestroyServer(entt::entity node);

	static void EnterScene(const EnterSceneParam& param);
	static void EnterDefaultScene(const EnterDefaultSceneParam& param);

	static void LeaveScene(const LeaveSceneParam& param);

	static void CompelPlayerChangeScene(const CompelChangeSceneParam& param);

	static void ReplaceCrashServer(entt::entity crash_node, entt::entity dest_node);

private:
	inline static ServerSequence24 server_sequence_;
};
