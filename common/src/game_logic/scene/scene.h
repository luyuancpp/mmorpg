#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "src/game_logic/scene/scene_factories.h"
#include "src/game_logic/game_registry.h"
#include "src/util/snow_flake.h"
#include "src/game_logic/scene/servernode_system.h"

struct EnterSceneParam
{
	entt::entity scene_{ entt::null };
	entt::entity enterer_{ entt::null };
};

struct LeaveSceneParam
{
	entt::entity leaver_{ entt::null };
};

class ScenesSystem : public EntityPtr
{
public:

    static ScenesSystem& GetSingleton() { thread_local ScenesSystem singleton; return singleton; }

    SceneComp copy_scenes_id() { return scenes_; }
    std::size_t scenes_size(uint32_t scene_config_id)const;
    std::size_t scenes_size() const { return scenes_.size(); }
    std::size_t scenes_map_size() const { return scenes_.size(); }
    void set_server_squence_node_id(uint32_t node_id) { server_squence_.set_node_id(node_id); }

    entt::entity get_scene(Guid scene_id);

    bool HasScene(uint32_t scene_config_id);
    inline bool Empty() const { return scenes_.empty(); }

    uint32_t CheckScenePlayerSize(entt::entity scene);

    entt::entity CreateScene(const CreateSceneP& param);
    entt::entity CreateSceneByGuid(const CreateSceneBySceneInfoP& param);
    entt::entity CreateScene2Gs(const CreateGsSceneP& param);

    void DestroyScene(const DestroySceneParam& param);
    void DestroyServer(const DestroyServerParam& param);

    void MoveServerScene2ServerScene(const MoveServerScene2ServerSceneP& param);

    void EnterScene(const EnterSceneParam& param);

    void LeaveScene(const LeaveSceneParam& param);

    void CompelChangeScene(const CompelChangeSceneParam& param);

    void ReplaceCrashServer(const ReplaceCrashServerParam& param);

    void LogPlayerEnterScene(entt::entity player);
    void LogPlayerLeaveScene(entt::entity player);
private:

    SceneComp scenes_;
    ServerSequence24 server_squence_;
};

