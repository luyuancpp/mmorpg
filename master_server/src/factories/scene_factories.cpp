#include "scene_factories.hpp"

#include "src/game_logic/game_registry.h"
#include "src/snow_flake/snow_flake.h"

namespace master
{
    entt::entity& scenes_entity()
    {
        thread_local entt::entity g_scenes_entity;
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
        auto scene_guid = reg.get<common::Guid>(scene_entity);
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
        reg.emplace<common::Guid>(e, scene_guid);
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


}//namespace master