#include "scene_factories.h"

#include "src/game_logic/game_registry.h"
#include "src/util/snow_flake.h"

entt::entity MakeMainSceneNode(const MakeGSParam& param)
{
    auto e = registry.create();
    AddMainSceneNodeCompnent(e, param);
    return e;
}

void AddMainSceneNodeCompnent(entt::entity server, const MakeGSParam& param)
{
	registry.emplace<MainSceneServer>(server);
	registry.emplace<GSNormal>(server);
	registry.emplace<NoPressure>(server);
	registry.emplace<ConfigSceneMap>(server);
	registry.emplace<GsNodePlayerInfoPtr>(server, std::make_shared<GsNodePlayerInfoPtr::element_type>());
}
