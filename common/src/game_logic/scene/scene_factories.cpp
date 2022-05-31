#include "scene_factories.h"

#include "src/game_logic/game_registry.h"
#include "src/util/snow_flake.h"


entt::entity& scenes_entity()
{
    thread_local entt::entity g_scenes_entity;
    return g_scenes_entity;
}

void MakeScenes()
{
    scenes_entity() = registry.create();
    registry.emplace<ConfigSceneMap>(scenes_entity());
    registry.emplace<SnowFlake>(scenes_entity());

}

entt::entity MakeMainSceneNode(entt::registry& reg,  const MakeGSParam& param)
{
    auto e = registry.create();
    AddMainSceneNodeCompnent(e, param);
    return e;
}

void AddMainSceneNodeCompnent(entt::entity e, const MakeGSParam& param)
{
	GsDataPtr p_server_data = std::make_shared<GsData>();
	p_server_data->set_node_id(param.node_id_);
	p_server_data->set_node_entity(e);
	registry.emplace<MainSceneServer>(e);
	registry.emplace<GsDataPtr>(e, p_server_data);
	registry.emplace<GSNormal>(e);
	registry.emplace<NoPressure>(e);
	registry.emplace<ConfigSceneMap>(e);
}
