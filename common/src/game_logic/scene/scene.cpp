#include "src/game_logic/scene/scene.h"

#include "muduo/base/Logging.h"

#include "src/game_logic/tips_id.h"
#include "src/game_logic/game_registry.h"

#include "component_proto/scene_comp.pb.h"

static const std::size_t kMaxMainScenePlayer = 1000;

void set_server_squence_node_id(uint32_t node_id) { ScenesSystem::GetSingleton().set_server_squence_node_id(node_id); }

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

entt::entity ScenesSystem::CreateScene(const CreateSceneP& param)
{
    CreateSceneBySceneInfoP param_by_guid;
    param_by_guid.scene_info_.set_scene_confid(param.scene_confid_);
    param_by_guid.scene_info_.set_scene_id(server_squence_.Generate());
    return CreateSceneByGuid(param_by_guid);
}

entt::entity ScenesSystem::CreateSceneByGuid(const CreateSceneBySceneInfoP& param)
{
	auto e = registry.create();
	auto& si = registry.emplace<SceneInfo>(e, param.scene_info_);
	registry.emplace<MainScene>(e);
	registry.emplace<ScenePlayers>(e);
	auto sit = scenes_map_.emplace(si.scene_id(), e);
	if (!sit.second)
	{
		LOG_ERROR << "already has scene" << si.scene_id();
	}

    return e;
}

entt::entity ScenesSystem::CreateScene2Gs(const CreateGsSceneP& param)
{
    CreateSceneP create_p;
    create_p.scene_confid_ = param.scene_confid_;
    auto scene = CreateScene(create_p);
	auto server = param.node_;
    auto try_server_player_info = registry.try_get<GsNodePlayerInfoPtr>(server);
    if (nullptr != try_server_player_info)
    {
        registry.emplace<GsNodePlayerInfoPtr>(scene, *try_server_player_info);
    }
	auto& server_scenes = registry.get<ConfigSceneMap>(server);
	server_scenes.AddScene(registry.get<SceneInfo>(scene).scene_confid(), scene);
    return scene;
}

void ScenesSystem::DestroyScene(const DestroySceneParam& param)
{
    auto scene_entity = param.scene_;
	auto& si = registry.get<SceneInfo>(scene_entity);
	scenes_map_.erase(si.scene_id());
	registry.destroy(scene_entity);
	auto& server_scene = registry.get<ConfigSceneMap>(param.server_);
	server_scene.RemoveScene(si.scene_confid(), scene_entity);
}

void ScenesSystem::DestroyServer(const DestroyServerParam& param)
{
    auto server_entity = param.server_;
    auto server_scenes = registry.get<ConfigSceneMap>(server_entity).scenesids_clone();
    DestroySceneParam destroy_param;
    destroy_param.server_ = server_entity;
    for (auto& it : server_scenes)
    {
        destroy_param.scene_ = it;
        DestroyScene(destroy_param);
    }
    registry.destroy(server_entity);
}

void ScenesSystem::MoveServerScene2ServerScene(const MoveServerScene2ServerSceneP& param)
{
    auto to_server_entity = param.to_server_;
    auto& from_scenes_ids = registry.get<ConfigSceneMap>(param.from_server_).confid_sceneslist();
    auto& to_scenes_id = registry.get<ConfigSceneMap>(to_server_entity);
    auto& p_to_server_data = registry.get<GsNodePlayerInfoPtr>(to_server_entity);
    for (auto& it : from_scenes_ids)
    {
        for (auto& ji : it.second)
        {
            registry.emplace_or_replace<GsNodePlayerInfoPtr>(ji, p_to_server_data);//todo 人数计算错误
            to_scenes_id.AddScene(it.first, ji);
        }
    }
    registry.emplace_or_replace<ConfigSceneMap>(param.from_server_);//todo 如果原来server 还有场景呢
}

uint32_t ScenesSystem::CheckScenePlayerSize(entt::entity scene)
{
    if (registry.get<ScenePlayers>(scene).size() >= kMaxMainScenePlayer)
    {
        return kRetEnterSceneNotFull;
    }
    auto p_gs_player_info = registry.try_get<GsNodePlayerInfoPtr>(scene);
    if (nullptr == p_gs_player_info)
    {
        LOG_ERROR << " gs null";
        return kRetEnterSceneGsInfoNull;
    }
    if ((*p_gs_player_info)->player_size() >= kMaxServerPlayerSize)
    {
        return kRetEnterSceneGsFull;
    }
    return kRetOK;
}

void ScenesSystem::EnterScene(const EnterSceneParam& param)
{
    auto scene_ = param.scene_;
    if (scene_ == entt::null)
    {
        LOG_INFO << "enter error" << entt::to_integral(param.enterer_);
        return;
    }
    registry.get<ScenePlayers>(scene_).emplace(param.enterer_);
    registry.emplace<SceneEntity>(param.enterer_, scene_);
    LogPlayerEnterScene(param.enterer_);
	auto p_gs_player_info = registry.try_get<GsNodePlayerInfoPtr>(scene_);
	if (nullptr == p_gs_player_info)
	{
		return;
	}
	(*p_gs_player_info)->set_player_size((*p_gs_player_info)->player_size() + 1);
}

void ScenesSystem::LeaveScene(const LeaveSceneParam& param)
{
    auto leave_player = param.leaver_;
    if (nullptr == registry.try_get<SceneEntity>(leave_player))
    {
        LOG_ERROR << "leave scene empty";
        return;
    }
    auto& player_scene_entity = registry.get<SceneEntity>(leave_player);
    auto scene_entity = player_scene_entity.scene_entity_;
    LogPlayerLeaveScene(leave_player);
    registry.get<ScenePlayers>(scene_entity).erase(leave_player);
    registry.remove<SceneEntity>(leave_player);
    auto p_gs_player_info = registry.try_get<GsNodePlayerInfoPtr>(scene_entity);
    if (nullptr == p_gs_player_info)
    {
        return;
    }
    (*p_gs_player_info)->set_player_size((*p_gs_player_info)->player_size() - 1);
}

void ScenesSystem::CompelChangeScene(const CompelChangeSceneParam& param)
{
    auto new_server_entity = param.new_server_;
    auto compel_entity = param.compel_change_player_;
    auto& new_server_scene = registry.get<ConfigSceneMap>(new_server_entity);
    auto scene_config_id = param.scene_confid_;
    entt::entity server_scene_enitity = entt::null;
    if (!new_server_scene.HasConfig(param.scene_confid_))
    {
        CreateGsSceneP create_gs_scene_param;
        create_gs_scene_param.scene_confid_ = scene_config_id;
        create_gs_scene_param.node_ = new_server_entity;
        server_scene_enitity = CreateScene2Gs(create_gs_scene_param);
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

void ScenesSystem::LogPlayerEnterScene(entt::entity player)
{
    auto try_player_id = registry.try_get<Guid>(player);
    if (nullptr == try_player_id)
    {
        return;
    }
    LOG_INFO << "player enter scene " << *try_player_id << " "
        << registry.get<SceneInfo>(registry.get<SceneEntity>(player).scene_entity_).scene_id();
}

void ScenesSystem::LogPlayerLeaveScene(entt::entity player)
{
    auto try_player_id = registry.try_get<Guid>(player);
    if (nullptr == try_player_id)
    {
        return;
    }
    LOG_INFO << "player leave scene " << *try_player_id << " "
        << registry.get<SceneInfo>(registry.get<SceneEntity>(player).scene_entity_).scene_id();
}

