#include "scene_factories.hpp"

#include "src/game_logic/game_registry.h"

namespace master
{
    static entt::entity g_scenes_entity;
    static entt::entity g_gameserver_entity;

    entt::entity& scenes_entity()
    {
        return g_scenes_entity;
    }

    void MakeScenes()
    {
        scenes_entity() = common::reg().create();
        common::reg().emplace<common::Scenes>(scenes_entity());
    }

    void OnCreateScene(entt::registry& reg, entt::entity scene_entity)
    {
        auto p_scene_entity = reg.try_get<common::SceneConfigId>(scene_entity);
        if (nullptr == p_scene_entity)
        {
            return;
        }
        uint32_t scene_config_id = p_scene_entity->scene_config_id();
        auto& c = reg.get<common::Scenes>(scenes_entity());
        c.AddScene(scene_config_id, scene_entity);
    }

    void OnDestroyScene(entt::registry& reg, entt::entity scene_entity, common::Scenes& scene_map)
    {
        auto scene_config_id = reg.get<common::SceneConfigId>(scene_entity).scene_config_id();
        scene_map.RemoveScene(scene_config_id, scene_entity);
        auto p_server_data = reg.get<common::GameServerDataPtr>(scene_entity);
        reg.destroy(scene_entity);
        if (nullptr == p_server_data)
        {
            return;
        }
        auto& server_scene = reg.get<common::SceneIds>(p_server_data->server_entity_);
        server_scene.erase(scene_entity);
    }

    entt::entity MakeMainScene(entt::registry& reg, const MakeSceneParam& param)
    {
        auto e = reg.create();
        reg.emplace<common::SceneConfigId>(e, param.scene_config_id_);
        reg.emplace<common::PlayerEntities>(e);
        OnCreateScene(reg, e);
        return e;
    }

    entt::entity MakeGameServer(entt::registry& reg,  const MakeGameServerParam& param)
    {
        auto e = reg.create();
        common::GameServerDataPtr p_server_data = std::make_shared<common::GameServerData>();
        p_server_data->server_id_ = param.server_id_;
        p_server_data->server_entity_ = e;
        reg.emplace<common::GameServerDataPtr>(e, p_server_data);
        reg.emplace<common::GameServerStatusNormal>(e);
        reg.emplace<common::SceneIds>(e);
        return e;
    }

    entt::entity MakeScene2GameServer(entt::registry& reg, const MakeScene2GameServerParam& param)
    {
        MakeSceneParam main_scene_param;
        main_scene_param.op_ = param.op_;
        main_scene_param.scene_config_id_ = param.scene_config_id_;
        auto e = MakeMainScene(reg, main_scene_param);
        PutScene2GameServerParam put_param;
        put_param.scene_entity_ = e;
        put_param.server_entity_ = param.server_entity_;
        PutScene2GameServer(reg, put_param);
        return e;
    }

    void PutScene2GameServer(entt::registry& reg, const PutScene2GameServerParam& param)
    {
        auto scene_entity = param.scene_entity_;
        auto server_entity = param.server_entity_;
        auto& server_ids = reg.get<common::SceneIds>(server_entity);
        server_ids.emplace(scene_entity);
        auto& p_server_data = reg.get<common::GameServerDataPtr>(server_entity);
        reg.emplace<common::GameServerDataPtr>(scene_entity, p_server_data);
    }

    void MoveServerScene2Server(entt::registry& reg, const MoveServerScene2ServerParam& param)
    {
        auto to_server_entity = param.to_server_entity_;
        auto& from_scenes_id = reg.get<common::SceneIds>(param.from_server_entity_);
        auto& to_scenes_id = reg.get<common::SceneIds>(to_server_entity);
        auto& p_to_server_data = reg.get<common::GameServerDataPtr>(to_server_entity);
     
        for (auto& it : from_scenes_id)
        {
            reg.emplace_or_replace<common::GameServerDataPtr>(it, p_to_server_data);
            to_scenes_id.emplace(it);
        }
        from_scenes_id.clear();
    }

    void DestroyScene(entt::registry& reg,  const DestroySceneParam& param)
    {
        auto& scene_map = reg.get<common::Scenes>(scenes_entity());
        OnDestroyScene(reg, param.scene_entity_, scene_map);
    }

    void DestroyServer(entt::registry& reg, const DestroyServerParam& param)
    {
        auto server_entity = param.server_entity_;
        auto server_scenes =  reg.get<common::SceneIds>(server_entity);
        auto& scene_map = reg.get<common::Scenes>(scenes_entity());
        DestroySceneParam destroy_param;
        for (auto& it : server_scenes)
        {
            OnDestroyScene(reg, it,  scene_map);
        }
        reg.destroy(server_entity);
    }

}//namespace master