#include "src/game_logic/scene/scene.h"

#include "muduo/base/Logging.h"

#include "src/game_logic/tips_id.h"
#include "src/game_logic/game_registry.h"

static const std::size_t kMaxMainScenePlayer = 1000;

SceneComp ScenesSystem::scenes_;
ServerSequence24 ScenesSystem::server_squence_;
ScenesSystem::scene_entity_cb ScenesSystem::before_enter_scene_cb_;
ScenesSystem::scene_entity_cb ScenesSystem::on_enter_scene_cb_;
ScenesSystem::scene_entity_cb ScenesSystem::before_leave_scene_cb_;
ScenesSystem::scene_entity_cb ScenesSystem::on_leave_scene_cb_;

void set_server_squence_node_id(uint32_t node_id) { ScenesSystem::set_server_squence_node_id(node_id); }

void AddMainSceneNodeCompnent(entt::entity server)
{
    registry.emplace<MainSceneServer>(server);
    registry.emplace<GSNormal>(server);
    registry.emplace<NoPressure>(server);
    registry.emplace<ConfigSceneMap>(server);
    registry.emplace<GsNodePlayerInfoPtr>(server, std::make_shared<GsNodePlayerInfoPtr::element_type>());
}

ScenesSystem::~ScenesSystem()
{
	scenes_.clear();
}

std::size_t ScenesSystem::scenes_size(uint32_t scene_config_id)
{
    std::size_t sz = 0;
    //todo  auto 
    for (auto& it : scenes_)
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
	auto it = scenes_.find(scene_id);
	if (it == scenes_.end())
	{
		return entt::null;
	}
	return it->second;
}

bool ScenesSystem::HasScene(uint32_t scene_config_id)
{
	for (auto& it : scenes_)
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
    CreateSceneBySceneInfoP create_by_guid_param;
    create_by_guid_param.scene_info_.set_scene_confid(param.scene_confid_);
    create_by_guid_param.scene_info_.set_scene_id(server_squence_.Generate());
    return CreateSceneByGuid(create_by_guid_param);
}

entt::entity ScenesSystem::CreateSceneByGuid(const CreateSceneBySceneInfoP& param)
{
	auto e = registry.create();
	auto& si = registry.emplace<SceneInfo>(e, param.scene_info_);
	registry.emplace<MainScene>(e);
	registry.emplace<ScenePlayers>(e);
	auto sit = scenes_.emplace(si.scene_id(), e);
	if (!sit.second)
	{
		LOG_ERROR << "already has scene" << si.scene_id();
	}
    return e;
}

entt::entity ScenesSystem::CreateScene2Gs(const CreateGsSceneP& param)
{
    CreateSceneP cp;
    cp.scene_confid_ = param.scene_confid_;
    auto scene = CreateScene(cp);
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
    // todo 人得换场景
    auto scene_entity = param.scene_;
	auto& si = registry.get<SceneInfo>(scene_entity);
	scenes_.erase(si.scene_id());
	registry.destroy(scene_entity);
	auto& server_scene = registry.get<ConfigSceneMap>(param.server_);
	server_scene.RemoveScene(si.scene_confid(), scene_entity);
}

void ScenesSystem::DestroyServer(const DestroyServerParam& param)
{
    // todo 人得换场景
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
    if (param.scene_ == entt::null)
    {
        LOG_INFO << "enter error" << entt::to_integral(param.enterer_);
        return;
    }
    if (before_enter_scene_cb_)
    {
        before_enter_scene_cb_(param.enterer_);
    }
    //todo gs 只要人数更改
    registry.get<ScenePlayers>(param.scene_).emplace(param.enterer_);
    registry.emplace<SceneEntity>(param.enterer_, param.scene_);
    //LogPlayerEnterScene(param.enterer_);
	auto try_gs_player_info = registry.try_get<GsNodePlayerInfoPtr>(param.scene_);
	if (nullptr != try_gs_player_info)
	{
        (*try_gs_player_info)->set_player_size((*try_gs_player_info)->player_size() + 1);
	}	
	if (on_enter_scene_cb_)
	{
        on_enter_scene_cb_(param.enterer_);
	}
}

void ScenesSystem::LeaveScene(const LeaveSceneParam& param)
{
    auto leaver = param.leaver_;
    if (nullptr == registry.try_get<SceneEntity>(leaver))
    {
        LOG_ERROR << "leave scene empty";
        return;
    }
    auto& player_scene = registry.get<SceneEntity>(leaver);
    auto scene = player_scene.scene_entity_;
	if (before_leave_scene_cb_)
	{
        before_leave_scene_cb_(leaver);
	}
    //LogPlayerLeaveScene(leaver);
    registry.get<ScenePlayers>(scene).erase(leaver);
    registry.remove<SceneEntity>(leaver);
    auto p_gs_player_info = registry.try_get<GsNodePlayerInfoPtr>(scene);
    if (nullptr != p_gs_player_info)
    {
        (*p_gs_player_info)->set_player_size((*p_gs_player_info)->player_size() - 1);
    }
	if (on_leave_scene_cb_)
	{
        on_leave_scene_cb_(leaver);
	}
}

void ScenesSystem::CompelToChangeScene(const CompelChangeSceneParam& param)
{
    auto new_server = param.new_server_;
    auto player = param.player_;
    auto& new_server_scene = registry.get<ConfigSceneMap>(new_server);
    auto scene_config_id = param.scene_confid_;
    entt::entity server_scene_enitity = entt::null;
    if (!new_server_scene.HasConfig(param.scene_confid_))
    {
        CreateGsSceneP create_gs_scene_param;
        create_gs_scene_param.scene_confid_ = scene_config_id;
        create_gs_scene_param.node_ = new_server;
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
    leave_param.leaver_ = player;
    LeaveScene(leave_param);

    EnterSceneParam enter_param;
    enter_param.enterer_ = player;
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

