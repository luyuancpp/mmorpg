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

struct CheckEnterSceneParam
{
	Guid scene_id_{ kInvalidGuid };
	entt::entity player_{ entt::null };
};

struct LeaveSceneParam
{
	entt::entity leaver_{ entt::null };
};

class ScenesSystem : public EntityPtr
{
public:

    static ScenesSystem& GetSingleton() { thread_local ScenesSystem singleton; return singleton; }

    SceneMapComp copy_scenes_id() { return scenes_map_; }
    std::size_t scenes_size(uint32_t scene_config_id)const;
    std::size_t scenes_size() const { return scenes_map_.size(); }
    std::size_t scenes_map_size() const { return scenes_map_.size(); }

    entt::entity get_scene(Guid scene_id);

    bool HasScene(uint32_t scene_config_id);
    inline bool Empty() const { return scenes_map_.empty(); }

    uint32_t CheckScenePlayerSize(const CheckEnterSceneParam& param);
    uint32_t CheckScenePlayerSize(entt::entity scene);

    entt::entity MakeScene(const MakeSceneP& param);
    entt::entity MakeSceneByGuid(const MakeSceneWithGuidP& param);
    entt::entity MakeScene2Gs(const MakeGsSceneP& param);

    void DestroyScene(const DestroySceneParam& param);
    void DestroyServer(const DestroyServerParam& param);

    void MoveServerScene2ServerScene(const MoveServerScene2ServerSceneP& param);

    void EnterScene(const EnterSceneParam& param);

    void LeaveScene(const LeaveSceneParam& param);

    void CompelChangeScene(const CompelChangeSceneParam& param);

    void ReplaceCrashServer(const ReplaceCrashServerParam& param);
private:

    SceneMapComp scenes_map_;
    SnowFlake snow_flake_;
};

