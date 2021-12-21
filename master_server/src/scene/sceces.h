#ifndef MASTER_SERVER_SRC_SCENE_SCENES_H_
#define MASTER_SERVER_SRC_SCENE_SCENES_H_

#include "entt/src/entt/entity/registry.hpp"

#include "src/factories/scene_factories.hpp"
#include "src/game_logic/entity_class/entity_class.h"
#include "src/game_logic/comp/gs_scene_comp.hpp"
#include "src/snow_flake/snow_flake.h"
#include "src/sys/scene_sys.hpp"

namespace master
{

class ScenesManager : public common::EntityHandle
{
public:
    ScenesManager();

    entt::entity first_scene() { if (scenes_.empty()) { return entt::null; } return *scenes_.begin(); }
    const common::EntitySet& scenes_id() const { return scenes_; }
    const common::Uint32KeyEntitySetValue& scenes_config_id() const { return confid_scenelist_; }
    const common::EntitySet& scenes_config_id(uint32_t scene_config_id) const;
    common::EntitySet copy_scenes_id() { return scenes_; }
    entt::entity scenelist(uint32_t scene_config_id)const;
    std::size_t confid_scenelist_size(uint32_t scene_config_id)const;
    std::size_t scenes_size() const { return scenes_.size(); }
    std::size_t scenes_map_size() const { return scenes_map_.size(); }

    bool IsConfigSceneEmpty(uint32_t scene_config_id);
    inline bool Empty() const { return scenes_.empty(); }
    inline bool HasSceneConfig(uint32_t scene_config_id) { return confid_scenelist_.find(scene_config_id) != confid_scenelist_.end(); }

    entt::entity MakeMainScene(const MakeMainSceneP& param);
    entt::entity MakeSceneGSScene(const MakeGSSceneP& param);

    void PutScene2GameServer(const PutScene2GSParam& param);

    void DestroyScene(const DestroySceneParam& param);
    void DestroyServer(const DestroyServerParam& param);

    void MoveServerScene2ServerScene(const MoveServerScene2ServerSceneP& param);

    void EnterScene(const EnterSceneParam& param);

    void LeaveScene(const LeaveSceneParam& param);

    void CompelChangeScene(const CompelChangeSceneParam& param);

    void ReplaceCrashServer(const ReplaceCrashServerParam& param);
private:

    void AddScene(uint32_t scene_config_id, entt::entity scene_entity);

    void RemoveScene(uint32_t scene_config_id, entt::entity scene_entity);

    void OnDestroyScene(entt::entity scene_entity);

    common::Uint32KeyEntitySetValue confid_scenelist_;
    common::EntitySet scenes_;
    common::SceneMapComp scenes_map_;
    common::SnowFlake snow_flake_;
};
}//namespace master

extern master::ScenesManager* g_scene_manager;

#endif//MASTER_SERVER_SRC_SCENE_SCENES_H_
