#ifndef MASTER_SERVER_SRC_SCENE_SCENES_H_
#define MASTER_SERVER_SRC_SCENE_SCENES_H_

#include "entt/src/entt/entity/registry.hpp"

#include "src/factories/scene_factories.hpp"
#include "src/game_logic/entity_class/entity_class.h"
#include "src/game_logic/comp/server_list.hpp"
#include "src/snow_flake/snow_flake.h"

namespace master
{

class ScenesManager : public common::EntityClass
{
public:
    ScenesManager();

    entt::entity first_scene() { if (scenes_.empty()) { return entt::null; } return *scenes_.begin(); }
    const common::SceneIds& scenes_id() const { return scenes_; }
    const common::ConfigScenes& scenes_config_id() const { return config_scene_; }
    const common::SceneIds& scenes_config_id(uint32_t scene_config_id) const;
    common::SceneIds copy_scenes_id() { return scenes_; }
    entt::entity scene_id(uint32_t scene_config_id)const;
    std::size_t scene_config_size(uint32_t scene_config_id)const;
    std::size_t scenes_size() const { return scenes_.size(); }
    std::size_t scenes_map_size() const { return scenes_map_.size(); }

    bool scene_config_empty(uint32_t scene_config_id);
    inline bool scenes_empty() const { return scenes_.empty(); }
    inline bool HasSceneConfig(uint32_t scene_config_id) { return config_scene_.find(scene_config_id) != config_scene_.end(); }

    entt::entity MakeMainScene(const MakeSceneParam& param);

    entt::entity MakeScene2GameServer(const MakeScene2GameServerParam& param);
    void PutScene2GameServer(const PutScene2GameServerParam& param);
private:

    void AddScene(uint32_t scene_config_id, entt::entity scene_entity);

    void RemoveScene(uint32_t scene_config_id, entt::entity scene_entity);

    void OnDestroyScene(entt::entity scene_entity);

    common::ConfigScenes config_scene_;
    common::SceneIds scenes_;
    common::SceneMap scenes_map_;
    common::SnowFlake snow_flake_;
};
}//namespace master

#endif//MASTER_SERVER_SRC_SCENE_SCENES_H_
