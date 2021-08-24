#include "scene_factories.hpp"

namespace master
{

    void OnAddScene(entt::registry& reg,
        entt::entity scene_map_entity,
        entt::entity scene_entity)
    {
        auto p_scene_entity = reg.try_get<common::SceneConfigId>(scene_entity);
        if (nullptr == p_scene_entity)
        {
            return;
        }
        uint32_t scene_config_id = p_scene_entity->scene_config_id_;
        auto& c = reg.get<common::Scenes>(scene_map_entity);
        c.scenes_group_[scene_config_id].emplace(scene_entity);
        c.scenes_.emplace(scene_entity);
    }

    entt::entity MakeScenes(entt::registry& reg)
    {
        auto e = reg.create();
        auto& c = reg.emplace<common::Scenes>(e);
        return e;
    }

    entt::entity MakeMainScene(entt::registry& reg,
        const MakeSceneParam& param)
    {
        auto e = reg.create();
        reg.emplace<common::SceneConfigId>(e, param.scene_config_id_);
        reg.emplace<common::ScenePlayerList>(e); 
        OnAddScene(reg, param.scene_map_entity_, e);
        return e;
    }

    entt::entity MakeGameServer(entt::registry& reg,
        const MakeGameServerParam& param)
    {
        auto e = reg.create();
        reg.emplace<common::GameServerData>(e, param.server_id_);
        reg.emplace<common::SceneIds>(e);
        return e;
    }

    entt::entity MakeScene2GameServer(entt::registry& reg, 
        const MakeScene2GameServerParam& param)
    {
        MakeSceneParam main_scene_param;
        main_scene_param.op_ = param.op_;
        main_scene_param.scene_config_id_ = param.scene_config_id_;
        main_scene_param.scene_map_entity_ = param.scene_map_entity_;
        auto p_server_ids = reg.try_get<common::SceneIds>(param.server_entity_);
        if (nullptr == p_server_ids)
        {
            return entt::null;
        }
        auto e = MakeMainScene(reg, main_scene_param);
        p_server_ids->emplace(e);
        return e;
    }

    entt::entity MakeDungeonScene(entt::registry& reg,
               const MakeSceneParam& param)
    {
        auto e = MakeMainScene(reg, param);
        return e;
    }

}//namespace master