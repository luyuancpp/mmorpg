#ifndef COMMON_SRC_SCENE_SCENE_SCENE_H_
#define COMMON_SRC_SCENE_SCENE_SCENE_H_

#include "entt/src/entt/entity/registry.hpp"

#include "src/game_logic/scene/scene_factories.h"
#include "src/game_logic/game_registry.h"
#include "src/util/snow_flake.h"
#include "src/game_logic/scene/servernode_sys.h"

class ScenesSystem : public common::EntityPtr
{
public:
    common::SceneMapComp copy_scenes_id() { return scenes_map_; }
    std::size_t scenes_size(uint32_t scene_config_id)const;
    std::size_t scenes_size() const { return scenes_map_.size(); }
    std::size_t scenes_map_size() const { return scenes_map_.size(); }

    entt::entity get_scene(common::Guid scene_id);

    bool HasScene(uint32_t scene_config_id);
    inline bool Empty() const { return scenes_map_.empty(); }
    inline bool HasConfig(uint32_t scene_config_id) { return confid_scenes_.find(scene_config_id) != confid_scenes_.end(); }

    entt::entity MakeScene(const MakeSceneP& param);
    entt::entity MakeSceneByGuid(const MakeSceneWithGuidP& param);
    entt::entity MakeSceneGSScene(const MakeGSSceneP& param);

    void PutScene2GS(const PutScene2GSParam& param);

    void DestroyScene(const DestroySceneParam& param);
    void DestroyServer(const DestroyServerParam& param);

    void MoveServerScene2ServerScene(const MoveServerScene2ServerSceneP& param);

    void EnterScene(const EnterSceneParam& param);

    void LeaveScene(const LeaveSceneParam& param);

    void CompelChangeScene(const CompelChangeSceneParam& param);

    void ReplaceCrashServer(const ReplaceCrashServerParam& param);
private:

    void OnDestroyScene(entt::entity scene_entity);

    common::Uint32KeyEntitySetValue confid_scenes_;
    common::SceneMapComp scenes_map_;
    common::SnowFlake snow_flake_;
};

extern ScenesSystem* g_scene_sys;

#endif//COMMON_SRC_SCENE_SCENE_SCENE_H_
