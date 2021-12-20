#include "scene_sys.hpp"

#include "src/game_logic/comp/gs_scene_comp.hpp"
#include "src/factories/scene_factories.hpp"

using namespace common;

namespace master
{
void EnterScene(entt::registry& reg, const EnterSceneParam& param)
{
    auto scene_entity = param.scene_entity_;
    auto& players =  reg.get<PlayersComp>(scene_entity);
    players.emplace(param.enter_entity_);
    reg.emplace<common::SceneEntity>(param.enter_entity_, scene_entity);
    auto p_gs_data_comp = reg.try_get<GSDataPtrComp>(scene_entity);
    if (nullptr == p_gs_data_comp)
    {
        return;
    }
    (*p_gs_data_comp)->OnPlayerEnter();
}

void LeaveScene(entt::registry& reg, const LeaveSceneParam& param)
{
    auto leave_entity = param.leave_entity_;
    auto& player_scene_entity = reg.get<common::SceneEntity>(leave_entity);
    auto scene_entity = player_scene_entity.scene_entity();
    auto& players = reg.get<PlayersComp>(scene_entity);
    players.erase(leave_entity);
    reg.remove<common::SceneEntity>(leave_entity);
    auto p_gs_data_comp = reg.try_get<GSDataPtrComp>(scene_entity);
    if (nullptr == p_gs_data_comp)
    {
        return;
    }
    (*p_gs_data_comp)->OnPlayerLeave();
}

template<typename ServerType,typename ServerStatus, typename ServerPressure>
entt::entity GetWeightRoundRobinMainSceneT(entt::registry& reg, const GetWeightRoundRobinSceneParam& param)
{
    auto scene_config_id = param.scene_config_id_;
    entt::entity server_entity{ entt::null };
    std::size_t min_player_size = UINT64_MAX;
    for (auto e : reg.view<ServerType, ServerStatus, ServerPressure>())
    {
        auto& scenes = reg.get<SceneComp>(e);
        if (!scenes.HasSceneConfig(scene_config_id))
        {
            continue;
        }
        auto& server_data = reg.get<GSDataPtrComp>(e);
        std::size_t server_player_size = (*server_data).player_size();
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
    for (auto& ji : scenes.confid_sceneslist(scene_config_id))
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

entt::entity GetWeightRoundRobinMainScene(entt::registry& reg, const GetWeightRoundRobinSceneParam& param)
{
    auto scene_entity = GetWeightRoundRobinMainSceneT<MainSceneServerComp, GSNormalComp, NoPressureComp>(reg, param);
    if (entt::null != scene_entity)
    {
        return scene_entity;
    }
    return GetWeightRoundRobinMainSceneT<MainSceneServerComp, GSNormalComp, PressureComp>(reg, param);
}

entt::entity GetWeightRoundRobinRoomScene(entt::registry& reg, const GetWeightRoundRobinSceneParam& param)
{
    auto scene_entity = GetWeightRoundRobinMainSceneT<RoomSceneServerComp, GSNormalComp, NoPressureComp>(reg, param);
    if (entt::null != scene_entity)
    {
        return scene_entity;
    }
    return GetWeightRoundRobinMainSceneT<RoomSceneServerComp, GSNormalComp, PressureComp>(reg, param);
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
    reg.remove<GSNormalComp>(param.crash_server_entity_);
    reg.emplace<GSCrashComp>(param.crash_server_entity_);
}



void ServerMaintain(entt::registry& reg, const MaintainServerParam& param)
{
    reg.remove<GSNormalComp>(param.maintain_server_entity_);
    reg.emplace<GSMainTainComp>(param.maintain_server_entity_);
}
}//namespace master

