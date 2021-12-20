#include "scene_factories.hpp"

#include "src/game_logic/game_registry.h"
#include "src/snow_flake/snow_flake.h"

using namespace common;

namespace master
{
    entt::entity& scenes_entity()
    {
        thread_local entt::entity g_scenes_entity;
        return g_scenes_entity;
    }

    void MakeScenes()
    {
        scenes_entity() = reg().create();
        reg().emplace<SceneComp>(scenes_entity());
        reg().emplace<SnowFlake>(scenes_entity());
        reg().emplace<SceneMapComp>(scenes_entity());
    }

    void OnDestroyScene(entt::registry& reg, entt::entity scene_entity, SceneComp& scene_map)
    {
        auto scene_config_id = reg.get<SceneConfigComp>(scene_entity);
        scene_map.RemoveScene(scene_config_id, scene_entity);
        auto scene_guid = reg.get<Guid>(scene_entity);
        reg.get<SceneMapComp>(scenes_entity()).erase(scene_guid);
        auto p_server_data = reg.get<GameServerDataPtr>(scene_entity);
        reg.destroy(scene_entity);
        if (nullptr == p_server_data)
        {
            return;
        }
        auto& server_scene = reg.get<SceneComp>(p_server_data->server_entity());
        server_scene.RemoveScene(scene_config_id, scene_entity);
    }

    entt::entity MakeMainScene(entt::registry& reg, const MakeSceneParam& param)
    {
        auto e = reg.create();
        reg.emplace<SceneConfigComp>(e, param.scene_config_id_);
        reg.emplace<MainScene>(e);
        reg.emplace<PlayersComp>(e);
        auto& scene_config = reg.get<SceneConfigComp>(e);
        auto& c = reg.get<SceneComp>(scenes_entity());
        auto& sn = reg.get<SnowFlake>(scenes_entity());
        auto scene_guid = sn.Generate();
        reg.emplace<Guid>(e, scene_guid);
        reg.get<SceneMapComp>(scenes_entity()).emplace(scene_guid, e);
        c.AddScene(scene_config, e);
        return e;
    }

    entt::entity MakeMainSceneGameServer(entt::registry& reg,  const MakeGameServerParam& param)
    {
        auto e = reg.create();
        GameServerDataPtr p_server_data = std::make_shared<GameServerData>();
        p_server_data->set_node_id(param.node_id_);
        p_server_data->set_node_entity(e);
        reg.emplace<MainSceneServer>(e);
        reg.emplace<GameServerDataPtr>(e, p_server_data);
        reg.emplace<GameServerStatusNormal>(e);
        reg.emplace<GameNoPressure>(e);
        reg.emplace<SceneComp>(e);
        return e;
    }


}//namespace master