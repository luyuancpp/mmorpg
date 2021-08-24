#include "scene_factories.hpp"

namespace master
{
    entt::entity MakeMainSceneMap(entt::registry& reg)
    {
        auto e = reg.create();
        auto& c = reg.emplace<common::Scenes>(e);
        return e;
    }

    entt::entity MakeMainScene(entt::registry& reg,
        const MakeSceneParam& param)
    {
        auto e = reg.create();

        auto scene_config_id = param.scene_config_id_;

        reg.emplace<common::SceneConfigId>(e, scene_config_id);
        reg.emplace<common::ScenePlayerList>(e); 
        reg.emplace<common::MainScene>(e);
 
        auto& c = reg.get<common::Scenes>(param.scene_map_entity_);
        c.scenes_group_[scene_config_id].emplace(e);
        c.scenes_.emplace(e);
        return e;
    }

    entt::entity MakeGameServer(entt::registry& reg,
        const MakeGameServerParam& param)
    {
        auto e = reg.create();
        reg.emplace<common::GameServerData>(e, param.server_id_);
        reg.emplace<common::SceneIds>(e, param.scenes_id_);
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
        reg.remove<common::MainScene>(e);
        reg.emplace<common::DungeonScene>(e);
        return e;
    }

}//namespace master