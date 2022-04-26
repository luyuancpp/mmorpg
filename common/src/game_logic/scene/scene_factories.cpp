#include "scene_factories.h"

#include "src/game_logic/game_registry.h"
#include "src/util/snow_flake.h"

using namespace common;


    entt::entity& scenes_entity()
    {
        thread_local entt::entity g_scenes_entity;
        return g_scenes_entity;
    }

    void MakeScenes()
    {
        scenes_entity() = reg.create();
        reg.emplace<ConfigSceneMap>(scenes_entity());
        reg.emplace<SnowFlake>(scenes_entity());

    }

    entt::entity MakeMainSceneNode(entt::registry& reg,  const MakeGSParam& param)
    {
        auto e = reg.create();
        AddMainSceneNodeCompnent(e, param);
        return e;
    }

	void AddMainSceneNodeCompnent(entt::entity e, const MakeGSParam& param)
	{
		GsDataPtr p_server_data = std::make_shared<GsData>();
		p_server_data->set_node_id(param.node_id_);
		p_server_data->set_node_entity(e);
		reg.emplace<MainSceneServer>(e);
		reg.emplace<GsDataPtr>(e, p_server_data);
		reg.emplace<GSNormal>(e);
		reg.emplace<NoPressure>(e);
		reg.emplace<ConfigSceneMap>(e);
	}
