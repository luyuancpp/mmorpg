#include "scene_factories.hpp"

#include "src/game_logic/game_registry.h"
#include "src/snow_flake/snow_flake.h"

namespace master
{
    entt::entity& scenes_entity()
    {
        static entt::entity g_scenes_entity;
        return g_scenes_entity;
    }

    void MakeScenes()
    {
        scenes_entity() = common::reg().create();
        common::reg().emplace<common::Scenes>(scenes_entity());
        common::reg().emplace<common::SnowFlake>(scenes_entity());
        common::reg().emplace<common::SceneMap>(scenes_entity());
    }

    void OnDestroyScene(entt::registry& reg, entt::entity scene_entity, common::Scenes& scene_map)
    {
        auto scene_config_id = reg.get<common::SceneConfig>(scene_entity).scene_config_id();
        scene_map.RemoveScene(scene_config_id, scene_entity);
        auto scene_guid = reg.get<common::GameGuid>(scene_entity);
        reg.get<common::SceneMap>(scenes_entity()).erase(scene_guid);
        auto p_server_data = reg.get<common::GameServerDataPtr>(scene_entity);
        reg.destroy(scene_entity);
        if (nullptr == p_server_data)
        {
            return;
        }
        auto& server_scene = reg.get<common::Scenes>(p_server_data->server_entity());
        server_scene.RemoveScene(scene_config_id, scene_entity);
    }

    entt::entity MakeMainScene(entt::registry& reg, const MakeSceneParam& param)
    {
        auto e = reg.create();
        reg.emplace<common::SceneConfig>(e, param.scene_config_id_);
        reg.emplace<common::MainScene>(e);
        reg.emplace<common::PlayerEntities>(e);
        auto& scene_config = reg.get<common::SceneConfig>(e);
        auto& c = reg.get<common::Scenes>(scenes_entity());
        auto& sn = reg.get<common::SnowFlake>(scenes_entity());
        auto scene_guid = sn.Generate();
        reg.emplace<common::GameGuid>(e, scene_guid);
        reg.get<common::SceneMap>(scenes_entity()).emplace(scene_guid, e);
        c.AddScene(scene_config.scene_config_id(), e);
        return e;
    }

    entt::entity MakeMainSceneGameServer(entt::registry& reg,  const MakeGameServerParam& param)
    {
        auto e = reg.create();
        common::GameServerDataPtr p_server_data = std::make_shared<common::GameServerData>();
        p_server_data->set_node_id(param.node_id_);
        p_server_data->set_node_entity(e);
        reg.emplace<common::MainSceneServer>(e);
        reg.emplace<common::GameServerDataPtr>(e, p_server_data);
        reg.emplace<common::GameServerStatusNormal>(e);
        reg.emplace<common::GameNoPressure>(e);
        reg.emplace<common::Scenes>(e);
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
        auto& scene_config = reg.get<common::SceneConfig>(scene_entity);
        auto server_entity = param.server_entity_;
        auto& server_scenes = reg.get<common::Scenes>(server_entity);
        server_scenes.AddScene(scene_config.scene_config_id(), scene_entity);
        auto& p_server_data = reg.get<common::GameServerDataPtr>(server_entity);
        reg.emplace<common::GameServerDataPtr>(scene_entity, p_server_data);
    }

    void MoveServerScene2Server(entt::registry& reg, const MoveServerScene2ServerParam& param)
    {
        auto to_server_entity = param.to_server_entity_;
        auto& from_scenes_id = reg.get<common::Scenes>(param.from_server_entity_).scenes_config_id();
        auto& to_scenes_id = reg.get<common::Scenes>(to_server_entity);
        auto& p_to_server_data = reg.get<common::GameServerDataPtr>(to_server_entity);
        for (auto& it : from_scenes_id)
        {
            for (auto& ji : it.second)
            {
                reg.emplace_or_replace<common::GameServerDataPtr>(ji, p_to_server_data);
                to_scenes_id.AddScene(it.first, ji);
            }            
        }
        reg.emplace_or_replace<common::Scenes>(param.from_server_entity_);
    }

    void DestroyScene(entt::registry& reg,  const DestroySceneParam& param)
    {
        auto& scene_map = reg.get<common::Scenes>(scenes_entity());
        OnDestroyScene(reg, param.scene_entity_, scene_map);
    }

    void DestroyServer(entt::registry& reg, const DestroyServerParam& param)
    {
        auto server_entity = param.server_entity_;
        auto server_scenes =  reg.get<common::Scenes>(server_entity).copy_scenes_id();
        auto& scene_map = reg.get<common::Scenes>(scenes_entity());
        DestroySceneParam destroy_param;
        for (auto& it : server_scenes)
        {
            OnDestroyScene(reg, it,  scene_map);
        }
        reg.destroy(server_entity);
    }

}//namespace master