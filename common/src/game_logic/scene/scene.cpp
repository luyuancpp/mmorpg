#include "src/game_logic/scene/scene.h"

#include "muduo/base/Logging.h"
#include "src/game_logic/tips_id.h"
#include "src/game_logic/thread_local/thread_local_storage.h"

#include "event_proto/scene_event.pb.h"
#include "server_player_proto/scene_server_player.pb.h"

static const std::size_t kMaxMainScenePlayer = 1000;

SceneList ScenesSystem::scene_list_;
ServerSequence24 ScenesSystem::server_squence_;

void set_server_squence_node_id(uint32_t node_id) { ScenesSystem::set_server_squence_node_id(node_id); }

void AddMainSceneNodeCompnent(entt::entity server)
{
    tls.registry.emplace<MainSceneServer>(server);
    tls.registry.emplace<GSNormal>(server);
    tls.registry.emplace<NoPressure>(server);
    tls.registry.emplace<ConfigSceneMap>(server);
    tls.registry.emplace<GsNodePlayerInfoPtr>(server, std::make_shared<GsNodePlayerInfoPtr::element_type>());
}

ScenesSystem::~ScenesSystem()
{
	scene_list_.clear();
}

std::size_t ScenesSystem::scenes_size(uint32_t scene_config_id)
{
    std::size_t sz = 0;
    //todo  auto 
    for (auto& it : scene_list_)
    {
        if (tls.registry.get<SceneInfo>(it.second).scene_confid() != scene_config_id)
        {
            continue;
        }
        ++sz;
    }
    return sz;
}

entt::entity ScenesSystem::get_scene(Guid scene_id)
{
	auto it = scene_list_.find(scene_id);
	if (it == scene_list_.end())
	{
		return entt::null;
	}
	return it->second;
}

bool ScenesSystem::HasScene(uint32_t scene_config_id)
{
	for (auto& it : scene_list_)
	{
		if (tls.registry.get<SceneInfo>(it.second).scene_confid() == scene_config_id)
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
	auto e = tls.registry.create();
	auto& si = tls.registry.emplace<SceneInfo>(e, param.scene_info_);
	tls.registry.emplace<MainScene>(e);
	tls.registry.emplace<ScenePlayers>(e);
	auto sit = scene_list_.emplace(si.scene_id(), e);
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
    auto try_server_player_info = tls.registry.try_get<GsNodePlayerInfoPtr>(server);
    if (nullptr != try_server_player_info)
    {
        tls.registry.emplace<GsNodePlayerInfoPtr>(scene, *try_server_player_info);
    }
	auto& server_scenes = tls.registry.get<ConfigSceneMap>(server);
	server_scenes.AddScene(tls.registry.get<SceneInfo>(scene).scene_confid(), scene);
    return scene;
}

void ScenesSystem::DestroyScene(const DestroySceneParam& param)
{
    // todo 人得换场景
    auto scene_entity = param.scene_;
	auto& si = tls.registry.get<SceneInfo>(scene_entity);
	scene_list_.erase(si.scene_id());
	tls.registry.destroy(scene_entity);
	auto& server_scene = tls.registry.get<ConfigSceneMap>(param.server_);
	server_scene.RemoveScene(si.scene_confid(), scene_entity);
}

void ScenesSystem::DestroyServer(const DestroyServerParam& param)
{
    // todo 人得换场景
    auto server_entity = param.server_;
	EntitySet server_scenes;
	for (auto& it : tls.registry.get<ConfigSceneMap>(server_entity).confid_sceneslist())
	{
		for (auto& ji : it.second)
		{
            server_scenes.emplace(ji);
		}
	}
    DestroySceneParam destroy_param;
    destroy_param.server_ = server_entity;
    for (auto& it : server_scenes)
    {
        destroy_param.scene_ = it;
        DestroyScene(destroy_param);
    }
    tls.registry.destroy(server_entity);
}

void ScenesSystem::MoveServerScene2ServerScene(const MoveServerScene2ServerSceneP& param)
{
    auto to_server_entity = param.to_server_;
    auto& from_scenes_ids = tls.registry.get<ConfigSceneMap>(param.from_server_).confid_sceneslist();
    auto& to_scenes_id = tls.registry.get<ConfigSceneMap>(to_server_entity);
    auto& p_to_server_data = tls.registry.get<GsNodePlayerInfoPtr>(to_server_entity);
    for (auto& it : from_scenes_ids)
    {
        for (auto& ji : it.second)
        {
            tls.registry.emplace_or_replace<GsNodePlayerInfoPtr>(ji, p_to_server_data);//todo 人数计算错误,没有加上原来场景的人数
            to_scenes_id.AddScene(it.first, ji);
        }
    }
    tls.registry.emplace_or_replace<ConfigSceneMap>(param.from_server_);//todo 如果原来server 还有场景呢
}

uint32_t ScenesSystem::CheckScenePlayerSize(entt::entity scene)
{
    if (tls.registry.get<ScenePlayers>(scene).size() >= kMaxMainScenePlayer)
    {
        return kRetEnterSceneNotFull;
    }
    auto p_gs_player_info = tls.registry.try_get<GsNodePlayerInfoPtr>(scene);
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
    {
        BeforeEnterScene before_enter_scene_event;
        before_enter_scene_event.set_entity(entt::to_integral(param.enterer_));
        tls.dispatcher.trigger(before_enter_scene_event);
    }
    
    //todo gs 只要人数更改
    tls.registry.get<ScenePlayers>(param.scene_).emplace(param.enterer_);
    tls.registry.emplace<SceneEntity>(param.enterer_, param.scene_);
	auto try_gs_player_info = tls.registry.try_get<GsNodePlayerInfoPtr>(param.scene_);// todo weak_ptr ?
	if (nullptr != try_gs_player_info)
	{
        (*try_gs_player_info)->set_player_size((*try_gs_player_info)->player_size() + 1);
	}	
    {
        OnEnterScene on_enter_scene_event;
        on_enter_scene_event.set_entity(entt::to_integral(param.enterer_));
        tls.dispatcher.trigger(on_enter_scene_event);
    }    
}

void ScenesSystem::LeaveScene(const LeaveSceneParam& param)
{
    auto leaver = param.leaver_;
    if (nullptr == tls.registry.try_get<SceneEntity>(leaver))
    {
        LOG_ERROR << "leave scene empty";
        return;
    }
    auto& player_scene = tls.registry.get<SceneEntity>(leaver);
    auto scene = player_scene.scene_entity_;
    {
        BeforeLeaveScene before_leave_scene_event;
        before_leave_scene_event.set_entity(entt::to_integral(leaver));
        tls.dispatcher.trigger(before_leave_scene_event);
    }
    
    tls.registry.get<ScenePlayers>(scene).erase(leaver);
    tls.registry.remove<SceneEntity>(leaver);
    auto p_gs_player_info = tls.registry.try_get<GsNodePlayerInfoPtr>(scene);
    if (nullptr != p_gs_player_info)
    {
        (*p_gs_player_info)->set_player_size((*p_gs_player_info)->player_size() - 1);
    }
    {
        OnLeaveScene on_leave_scene_event;
        on_leave_scene_event.set_entity(entt::to_integral(leaver));
        tls.dispatcher.trigger(on_leave_scene_event);
    }    
}

void ScenesSystem::CompelToChangeScene(const CompelChangeSceneParam& param)
{
    auto new_server = param.new_server_;
    auto player = param.player_;
    auto& new_server_scene = tls.registry.get<ConfigSceneMap>(new_server);
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
    tls.registry.destroy(move_param.from_server_);
}

