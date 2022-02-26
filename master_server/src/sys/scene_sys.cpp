#include "scene_sys.hpp"

#include "src/game_logic/comp/gs_scene_comp.hpp"
#include "src/game_logic/game_registry.h"
#include "src/factories/scene_factories.hpp"

using namespace common;

namespace master
{
template<typename ServerType,typename ServerStatus, typename ServerPressure>
entt::entity GetWeightRoundRobinSceneT(const GetWeightRoundRobinSceneParam& param)
{
    auto scene_config_id = param.scene_confid_;
    entt::entity server_entity{ entt::null };
    std::size_t min_player_size = UINT64_MAX;

    for (auto e : reg.view<ServerType, ServerStatus, ServerPressure>())
    {
        if (!reg.get<SceneComp>(e).HasConfig(scene_config_id))
        {
            continue;
        }
        std::size_t server_player_size = (*reg.get<GSDataPtrComp>(e)).player_size();
        if (server_player_size >= min_player_size)
        {
            continue;
        }
        server_entity = e;
        min_player_size = server_player_size;   
    }
    entt::entity scene_entity{ entt::null };
    if (entt::null == server_entity)
    {
        return scene_entity;
    }
    auto& scenes = reg.get<SceneComp>(server_entity);
    std::size_t scene_min_player_size = UINT64_MAX;
    auto& server_scenes = scenes.confid_sceneslist(scene_config_id);
    for (auto& ji : server_scenes)
    {
        std::size_t scene_player_size = reg.get<PlayersComp>(ji).size();
        if (scene_player_size >= scene_min_player_size)
        {
            continue;
        }
        scene_min_player_size = scene_player_size;
        scene_entity = ji;
    }
    return scene_entity;
}

entt::entity GetWeightRoundRobinMainScene(const GetWeightRoundRobinSceneParam& param)
{
    auto scene_entity = GetWeightRoundRobinSceneT<MainSceneServerComp, GSNormalComp, NoPressureComp>( param);
    if (entt::null != scene_entity)
    {
        return scene_entity;
    }
    return GetWeightRoundRobinSceneT<MainSceneServerComp, GSNormalComp, PressureComp>( param);
}

entt::entity GetWeightRoundRobinRoomScene(const GetWeightRoundRobinSceneParam& param)
{
    auto scene_entity = GetWeightRoundRobinSceneT<RoomSceneServerComp, GSNormalComp, NoPressureComp>( param);
    if (entt::null != scene_entity)
    {
        return scene_entity;
    }
    return GetWeightRoundRobinSceneT<RoomSceneServerComp, GSNormalComp, PressureComp>(param);
}

void ServerEnterPressure(entt::registry& reg, const ServerPressureParam& param)
{
    reg.remove<NoPressureComp>(param.server_entity_);
    reg.emplace<PressureComp>(param.server_entity_);
}

void ServerEnterNoPressure(entt::registry& reg, const ServerPressureParam& param)
{
    reg.remove<PressureComp>(param.server_entity_);
    reg.emplace<NoPressureComp>(param.server_entity_);
}

void ServerCrashed(entt::registry& reg, const ServerCrashParam& param)
{
    reg.remove<GSNormalComp>(param.crash_entity_);
    reg.emplace<GSCrashComp>(param.crash_entity_);
}

void ServerMaintain(entt::registry& reg, const MaintainServerParam& param)
{
    reg.remove<GSNormalComp>(param.maintain_entity_);
    reg.emplace<GSMainTainComp>(param.maintain_entity_);
}
}//namespace master

