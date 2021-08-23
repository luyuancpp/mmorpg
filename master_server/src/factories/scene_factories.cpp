#include "scene_factories.hpp"

namespace master
{
    entt::entity MakeMainSceneMap(entt::registry& reg)
    {
        auto e = reg.create();
        auto& c = reg.emplace<common::SceneMap>(e);
        return e;
    }

    entt::entity MakeMainScene(entt::registry& reg,
        const MakeMainSceneParam& param)
    {
        auto e = reg.create();

        auto scene_config_id = param.scene_config_id_;

        reg.emplace<common::SceneConfigId>(e, scene_config_id);
        reg.emplace<common::ScenePlayerList>(e); 
 
        auto& c = reg.get<common::SceneMap>(param.scene_map_entity_);
        c.scenes_group_[scene_config_id].emplace(e);
        c.scenes_.emplace(e);
        return e;
    }

    entt::entity MakeServerScene(entt::registry& reg,
        const MakeServerSceneParam& param)
    {
        auto e = reg.create();
        reg.emplace<common::GameServerData>(e, param.server_id_);
        if (param.scenes_id_.empty())
        {
            auto& c = reg.get<common::SceneMap>(param.scene_map_entity_);
            reg.emplace<common::SceneIds>(e, c.scenes_group_[param.scene_config_id_]);
        }
        else
        {
            reg.emplace<common::SceneIds>(e, param.scenes_id_);
        }
        return e;
    }

}//namespace master