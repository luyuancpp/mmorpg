#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "src/game_logic/scene/scene_factories.h"
#include "src/game_logic/game_registry.h"
#include "src/util/snow_flake.h"
#include "src/game_logic/scene/servernode_sys.h"

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

    entt::entity MakeScene(const MakeSceneP& param);
    entt::entity MakeSceneByGuid(const MakeSceneWithGuidP& param);
    entt::entity MakeScene2Gs(const MakeGSSceneP& param);

    void PutScene2Gs(const PutScene2GSParam& param);

    void DestroyScene(const DestroySceneParam& param);
    void DestroyServer(const DestroyServerParam& param);

    void MoveServerScene2ServerScene(const MoveServerScene2ServerSceneP& param);

    uint32_t CheckEnterSceneByGuid(const CheckEnterSceneParam& param);
    void EnterScene(const EnterSceneParam& param);

    void LeaveScene(const LeaveSceneParam& param);

    void CompelChangeScene(const CompelChangeSceneParam& param);

    void ReplaceCrashServer(const ReplaceCrashServerParam& param);
private:

    void OnDestroyScene(entt::entity scene_entity);

    Uint32KeyEntitySetValue confid_scenes_;
    SceneMapComp scenes_map_;
    SnowFlake snow_flake_;
};

