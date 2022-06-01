#include "src/game_logic/scene/scene.h"

#include "muduo/base/Logging.h"

#include "src/game_logic/tips_id.h"
#include "src/game_logic/game_registry.h"

#include "src/pb/pbc/component_proto/scene_comp.pb.h"

static const std::size_t kMaxMainScenePlayer = 1000;

std::size_t ScenesSystem::scenes_size(uint32_t scene_config_id)const
{
    std::size_t sz = 0;
    //todo  auto 
    for (auto& it : scenes_map_)
    {
        if (registry.get<SceneInfo>(it.second).scene_confid() != scene_config_id)
        {
            continue;
        }
        ++sz;
    }
    return sz;
}

entt::entity ScenesSystem::get_scene(Guid scene_id)
{
    auto it = scenes_map_.find(scene_id);
    if (it == scenes_map_.end())
    {
        return entt::null;
    }
    return it->second;
}

bool ScenesSystem::HasScene(uint32_t scene_config_id)
{
	for (auto& it : scenes_map_)
	{
		if (registry.get<SceneInfo>(it.second).scene_confid() == scene_config_id)
		{
            return true;
		}
	}
	return false;
}

entt::entity ScenesSystem::MakeScene(const MakeSceneP& param)
{
    auto scene = registry.create();
    auto& si = registry.emplace<SceneInfo>(scene);
    si.set_scene_confid(param.scene_confid_);
    registry.emplace<MainScene>(scene);
    registry.emplace<ScenePlayers>(scene);
    auto guid = snow_flake_.Generate();
    si.set_scene_id(guid);
    auto sit = scenes_map_.emplace(guid, scene);
	if (!sit.second)
	{
        LOG_ERROR << "already has scene" << guid;
	}

    return scene;
}

entt::entity ScenesSystem::MakeSceneByGuid(const MakeSceneWithGuidP& param)
{
	auto e = registry.create();
    auto guid = param.scene_id;
	auto& si = registry.emplace<SceneInfo>(e);
    si.set_scene_confid(param.scene_confid_);
    si.set_scene_id(guid);
	registry.emplace<MainScene>(e);
	registry.emplace<ScenePlayers>(e);
	auto sit = scenes_map_.emplace(guid, e);
	if (!sit.second)
	{
		LOG_ERROR << "already has scene" << guid;
	}

    return e;
}

entt::entity ScenesSystem::MakeScene2Gs(const MakeGSSceneP& param)
{
    MakeSceneP make_p;
    make_p.scene_confid_ = param.scene_confid_;
    auto e = MakeScene(make_p);
    PutScene2GSParam put_param;
    put_param.scene_ = e;
    put_param.server_ = param.server_;
    PutScene2Gs(put_param);
    return e;
}

void ScenesSystem::PutScene2Gs(const PutScene2GSParam& param)
{
    auto scene_entity = param.scene_;
    auto server_entity = param.server_;
    auto& server_scenes = registry.get<ConfigSceneMap>(server_entity);
    server_scenes.AddScene(registry.get<SceneInfo>(scene_entity).scene_confid(), scene_entity);
    auto& p_server_data = registry.get<GsDataPtr>(server_entity);
    registry.emplace<GsDataPtr>(scene_entity, p_server_data);
}


void ScenesSystem::DestroyScene(const DestroySceneParam& param)
{
    OnDestroyScene(param.scene_);
}

void ScenesSystem::DestroyServer(const DestroyServerParam& param)
{
    auto server_entity = param.server_;
    auto server_scenes = registry.get<ConfigSceneMap>(server_entity).scenesids_clone();
    DestroySceneParam destroy_param;
    for (auto& it : server_scenes)
    {
        OnDestroyScene(it);
    }
    registry.destroy(server_entity);
}

void ScenesSystem::MoveServerScene2ServerScene(const MoveServerScene2ServerSceneP& param)
{
    auto to_server_entity = param.to_server_;
    auto& from_scenes_id = registry.get<ConfigSceneMap>(param.from_server_).confid_sceneslist();
    auto& to_scenes_id = registry.get<ConfigSceneMap>(to_server_entity);
    auto& p_to_server_data = registry.get<GsDataPtr>(to_server_entity);
    for (auto& it : from_scenes_id)
    {
        for (auto& ji : it.second)
        {
            registry.emplace_or_replace<GsDataPtr>(ji, p_to_server_data);
            to_scenes_id.AddScene(it.first, ji);
        }
    }
    registry.emplace_or_replace<ConfigSceneMap>(param.from_server_);
}

uint32_t ScenesSystem::CheckEnterSceneByGuid(const CheckEnterSceneParam& param)
{
    auto scene = get_scene(param.scene_id_);
    if (scene == entt::null)
    {
        return kRetEnterSceneNotFound;
    }
    if (registry.get<ScenePlayers>(scene).size() >= kMaxMainScenePlayer)
    {
        return kRetEnterSceneNotFull;
    }
    return kRetOK;
}

void ScenesSystem::EnterScene(const EnterSceneParam& param)
{
    auto scene_entity = param.scene_;
    if (scene_entity == entt::null)
    {
        LOG_INFO << "enter error" << entt::to_integral(param.enterer_);
        return;
    }
    registry.get<ScenePlayers>(scene_entity).emplace(param.enterer_);
    registry.emplace<SceneEntity>(param.enterer_, scene_entity);
    auto p_server_data = registry.try_get<GsDataPtr>(scene_entity);
    if (nullptr == p_server_data)
    {
        return;
    }
    (*p_server_data)->OnPlayerEnter();
}

void ScenesSystem::LeaveScene(const LeaveSceneParam& param)
{
    auto leave_player = param.leaver_;
    auto& player_scene_entity = registry.get<SceneEntity>(leave_player);
    auto scene_entity = player_scene_entity.scene_entity();
    registry.get<ScenePlayers>(scene_entity).erase(leave_player);
    registry.remove<SceneEntity>(leave_player);
    auto p_server_data = registry.try_get<GsDataPtr>(scene_entity);
    if (nullptr == p_server_data)
    {
        return;
    }
    (*p_server_data)->OnPlayerLeave();
}

void ScenesSystem::CompelChangeScene(const CompelChangeSceneParam& param)
{
    auto new_server_entity = param.new_server_;
    auto compel_entity = param.compel_change_entity_;
    auto& new_server_scene = registry.get<ConfigSceneMap>(new_server_entity);
    auto scene_config_id = param.scene_confid_;
    entt::entity server_scene_enitity = entt::null;
    if (!new_server_scene.HasConfig(param.scene_confid_))
    {
        MakeGSSceneP make_server_scene_param;
        make_server_scene_param.scene_confid_ = scene_config_id;
        make_server_scene_param.server_ = new_server_entity;
        server_scene_enitity = MakeScene2Gs(make_server_scene_param);
    }
    else
    {
        server_scene_enitity = new_server_scene.scenelist(param.scene_confid_);
    }

    if (entt::null == server_scene_enitity)
    {
        //todo 
        return;
    }

    LeaveSceneParam leave_param;
    leave_param.leaver_ = compel_entity;
    LeaveScene(leave_param);

    EnterSceneParam enter_param;
    enter_param.enterer_ = compel_entity;
    enter_param.scene_ = server_scene_enitity;
    EnterScene(enter_param);
}

void ScenesSystem::ReplaceCrashServer(const ReplaceCrashServerParam& param)
{
    MoveServerScene2ServerSceneP move_param;
    move_param.from_server_ = param.cransh_server_;
    move_param.to_server_ = param.replace_server_;
    MoveServerScene2ServerScene(move_param);
    registry.destroy(move_param.from_server_);
}

void ScenesSystem::OnDestroyScene(entt::entity scene_entity)
{
    auto& si = registry.get<SceneInfo>(scene_entity);
    scenes_map_.erase(si.scene_id());
    auto p_server_data = registry.get<GsDataPtr>(scene_entity);
    registry.destroy(scene_entity);
    if (nullptr == p_server_data)
    {
        return;
    }
    auto& server_scene = registry.get<ConfigSceneMap>(p_server_data->server_entity());
    server_scene.RemoveScene(si.scene_confid(), scene_entity);
}
