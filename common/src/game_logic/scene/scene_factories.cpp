#include "scene_factories.h"

#include "src/game_logic/game_registry.h"
#include "src/util/snow_flake.h"

entt::entity MakeMainSceneNode(const MakeGSParam& param)
{
    auto e = registry.create();
    AddMainSceneNodeCompnent(e, param);
    return e;
}

void AddMainSceneNodeCompnent(entt::entity e, const MakeGSParam& param)
{
	registry.emplace<MainSceneServer>(e);
	registry.emplace<GSNormal>(e);
	registry.emplace<NoPressure>(e);
	registry.emplace<ConfigSceneMap>(e);
}
