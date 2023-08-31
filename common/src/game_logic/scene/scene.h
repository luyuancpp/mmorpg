#pragma once

#include "src/game_logic/comp/server_comp.h"

#include "src/game_logic/scene/servernode_system.h"
#include "src/util/snow_flake.h"
#include <component_proto/scene_comp.pb.h>

struct SceneEntity
{
	entt::entity scene_entity_{entt::null };
};

struct EnterSceneParam
{
	inline bool IsNull() const
	{
		return scene_ == entt::null || enterer_ == entt::null;
	}

	entt::entity scene_{entt::null};
	entt::entity enterer_{entt::null};
};

struct LeaveSceneParam
{
	inline bool IsNull() const
	{
		return leaver_ == entt::null;
	}

	entt::entity leaver_{entt::null};
};

struct CreateSceneP
{
	uint32_t scene_confid_{0};
};

struct CreateSceneBySceneInfoP
{
	SceneInfo scene_info_;
};

struct CreateGsSceneP
{
	inline bool IsNull() const
	{
		return node_ == entt::null;
	}

	entt::entity node_{entt::null};
	uint32_t scene_confid_{0};
};

struct MoveServerScene2ServerSceneP
{
	inline bool IsNull() const
	{
		return src_node_ == entt::null || dest_node_ == entt::null;
	}

	entt::entity src_node_{entt::null};
	entt::entity dest_node_{entt::null};
};

struct DestroySceneParam
{
	inline bool IsNull() const
	{
		return scene_ == entt::null || node_ == entt::null;
	}

	entt::entity scene_{entt::null};
	entt::entity node_{entt::null};
};

struct DestroyServerParam
{
	inline bool IsNull() const
	{
		return node_ == entt::null;
	}
    entt::entity node_{ entt::null };
};

void AddMainSceneNodeComponent(entt::entity server);


//todo 1线镜像和一线应该是同一个gs,这样就不会有切换服务器的开销
class ScenesSystem 
{
public:

    ~ScenesSystem();//just for test clear 

    static std::size_t scenes_size(uint32_t scene_config_id);
    static std::size_t scenes_size();
    static void set_server_sequence_node_id(uint32_t node_id) { server_sequence_.set_node_id(node_id); }
	
    static bool HasConfigScene(uint32_t scene_config_id);
    static bool IsSceneEmpty();

    static uint32_t CheckScenePlayerSize(entt::entity scene);

    static entt::entity CreateSceneByGuid(const CreateSceneBySceneInfoP& param);
    static entt::entity CreateScene2Gs(const CreateGsSceneP& param);

    static void DestroyScene(const DestroySceneParam& param);
    static void DestroyServer(const DestroyServerParam& param);

    static void MoveServerScene2ServerScene(const MoveServerScene2ServerSceneP& param);

    static void EnterScene(const EnterSceneParam& param);

    static void LeaveScene(const LeaveSceneParam& param);

    static void CompelToChangeScene(const CompelChangeSceneParam& param);

    static void ReplaceCrashServer(const ReplaceCrashServerParam& param);

private:
	static inline ServerSequence24 server_sequence_;
};
