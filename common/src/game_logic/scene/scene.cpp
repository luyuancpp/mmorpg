#include "src/game_logic/scene/scene.h"

#include "muduo/base/Logging.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/game_logic/tips_id.h"

#include "component_proto/gs_node_comp.pb.h"
#include "event_proto/scene_event.pb.h"

static constexpr std::size_t kMaxMainScenePlayer = 1000;

using GsNodePlayerInfoPtr = std::shared_ptr<GsNodePlayerInfo>;

void set_server_sequence_node_id(uint32_t node_id) { ScenesSystem::set_server_sequence_node_id(node_id); }

void AddMainSceneNodeComponent(const entt::entity server)
{
	tls.registry.emplace<MainSceneServer>(server);
	tls.registry.emplace<ServerComp>(server);
	tls.registry.emplace<GsNodePlayerInfoPtr>(server, std::make_shared<GsNodePlayerInfoPtr::element_type>());
}

ScenesSystem::~ScenesSystem()
{
	scene_list_.clear();
}

std::size_t ScenesSystem::scenes_size(uint32_t scene_config_id)
{
	std::size_t scene_size = 0;
	//todo  auto
	for (const auto& val : scene_list_ | std::views::values)
	{
		if (tls.registry.get<SceneInfo>(val).scene_confid() != scene_config_id)
		{
			continue;
		}
		++scene_size;
	}
	return scene_size;
}

entt::entity ScenesSystem::get_scene(Guid scene_id)
{
	const auto scene_it = scene_list_.find(scene_id);
	if (scene_it == scene_list_.end())
	{
		return entt::null;
	}
	return scene_it->second;
}

bool ScenesSystem::HasScene(uint32_t scene_config_id)
{
	for (const auto& val : scene_list_ | std::views::values)
	{
		if (tls.registry.get<SceneInfo>(val).scene_confid() == scene_config_id)
		{
			return true;
		}
	}
	return false;
}

entt::entity ScenesSystem::CreateSceneByGuid(const CreateSceneBySceneInfoP& param)
{
	auto entity = tls.registry.create();
	const auto& scene_info = tls.registry.emplace<SceneInfo>(entity, param.scene_info_);
	tls.registry.emplace<ScenePlayers>(entity);
	if (const auto [fst, snd] = scene_list_.emplace(scene_info.scene_id(), entity);
		!snd)
	{
		LOG_ERROR << "already has scene" << scene_info.scene_id();
	}
	return entity;
}

entt::entity ScenesSystem::CreateScene2Gs(const CreateGsSceneP& param)
{
	if (param.IsNull())
	{
		LOG_ERROR << "server id error" << param.scene_confid_;
		return entt::null;
	}

	CreateSceneBySceneInfoP create_by_guid_param;
	create_by_guid_param.scene_info_.set_scene_confid(param.scene_confid_);
	create_by_guid_param.scene_info_.set_scene_id(server_sequence_.Generate());
	const auto scene_entity = CreateSceneByGuid(create_by_guid_param);

	if (auto* try_server_player_info = tls.registry.try_get<GsNodePlayerInfoPtr>(param.node_))
	{
		tls.registry.emplace<GsNodePlayerInfoPtr>(scene_entity, *try_server_player_info);
	}

	auto& server_scenes = tls.registry.get<ServerComp>(param.node_);
	server_scenes.AddScene(tls.registry.get<SceneInfo>(scene_entity).scene_confid(), scene_entity);

	return scene_entity;
}

void ScenesSystem::DestroyScene(const DestroySceneParam& param)
{
	if (param.IsNull())
	{
		LOG_ERROR << "entity null";
		return;
	}
	// todo 人得换场景
	auto& si = tls.registry.get<SceneInfo>(param.scene_);
	scene_list_.erase(si.scene_id());
	tls.registry.destroy(param.scene_);
	auto& server_scene = tls.registry.get<ServerComp>(param.node_);
	server_scene.RemoveScene(si.scene_confid(), param.scene_);
}

void ScenesSystem::DestroyServer(const DestroyServerParam& param)
{
	if (param.IsNull())
	{
		LOG_ERROR << "entity null";
		return;
	}
    // todo 人得换场景
	EntitySet server_scenes_set;
	for (auto& it : tls.registry.get<ServerComp>(param.node_).GetConfidScenesList())
	{
		for (auto& ji : it.second)
		{
            server_scenes_set.emplace(ji);
		}
	}
    DestroySceneParam dp;
    dp.node_ = param.node_;
    for (auto& it : server_scenes_set)
    {
        dp.scene_ = it;
        DestroyScene(dp);
    }
    tls.registry.destroy(param.node_);
}

void ScenesSystem::MoveServerScene2ServerScene(const MoveServerScene2ServerSceneP& param)
{
	if (param.IsNull())
	{
		LOG_ERROR << "entity null";
		return;
	}
	const auto& src_server_comp = tls.registry.get<ServerComp>(param.src_node_).GetConfidScenesList();
	auto& dest_server_comp = tls.registry.get<ServerComp>(param.dest_node_);
	auto* const p_to_server_data = tls.registry.try_get<GsNodePlayerInfoPtr>(param.dest_node_);
	if (nullptr == p_to_server_data)
	{
		return;
	}
	for (const auto& [fst, snd] : src_server_comp)
	{
		for (const auto& ji : snd)
		{
			tls.registry.emplace_or_replace<GsNodePlayerInfoPtr>(ji, *p_to_server_data); //todo 人数计算错误,没有加上原来场景的人数
			dest_server_comp.AddScene(fst, ji);
		}
	}
	tls.registry.emplace_or_replace<ServerComp>(param.src_node_);//todo 如果原来server 还有场景呢
}

uint32_t ScenesSystem::CheckScenePlayerSize(entt::entity scene)
{
    //todo weak ptr ?
    if (tls.registry.get<ScenePlayers>(scene).size() >= kMaxMainScenePlayer)
    {
        return kRetEnterSceneNotFull;
    }
    auto try_gs_player_info = tls.registry.try_get<GsNodePlayerInfoPtr>(scene);
    if (nullptr == try_gs_player_info)
    {
        LOG_ERROR << " gs null";
        return kRetEnterSceneGsInfoNull;
    }
    if ((*try_gs_player_info)->player_size() >= kMaxServerPlayerSize)
    {
        return kRetEnterSceneGsFull;
    }
    return kRetOK;
}

void ScenesSystem::EnterScene(const EnterSceneParam& param)
{
    if (param.IsNull())
    {
        LOG_INFO << "param null error";
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
	// todo weak_ptr ?
	if (auto try_gs_player_info = tls.registry.try_get<GsNodePlayerInfoPtr>(param.scene_))
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
	if (param.IsNull())
	{
		LOG_ERROR << "entity null";
		return;
	}
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
   
    if (auto try_gs_player_info = tls.registry.try_get<GsNodePlayerInfoPtr>(scene))
    {
        (*try_gs_player_info)->set_player_size((*try_gs_player_info)->player_size() - 1);
    }
    {
        OnLeaveScene on_leave_scene_event;
        on_leave_scene_event.set_entity(entt::to_integral(leaver));
        tls.dispatcher.trigger(on_leave_scene_event);
    }    
}

void ScenesSystem::CompelToChangeScene(const CompelChangeSceneParam& param)
{
    auto& new_server_scene = tls.registry.get<ServerComp>(param.new_server_);
    entt::entity server_scene_enitity = entt::null;
    if (!new_server_scene.HasConfig(param.scene_confid_))
    {
        CreateGsSceneP create_gs_scene_param;
        create_gs_scene_param.scene_confid_ = param.scene_confid_;
        create_gs_scene_param.node_ = param.new_server_;
        server_scene_enitity = CreateScene2Gs(create_gs_scene_param);
    }
	else
	{
        //todo 第一个 场景压力会特别大
		server_scene_enitity = new_server_scene.GetFirstSceneByConfigId(param.scene_confid_);
	}

    if (entt::null == server_scene_enitity)
    {
        //todo 
        return;
    }

    LeaveSceneParam leave_param;
    leave_param.leaver_ = param.player_;
    LeaveScene(leave_param);

    EnterSceneParam enter_param;
    enter_param.enterer_ = param.player_;
    enter_param.scene_ = server_scene_enitity;
    EnterScene(enter_param);
}

void ScenesSystem::ReplaceCrashServer(const ReplaceCrashServerParam& param)
{
    if (param.IsNull())
    {
        return;
    }
    MoveServerScene2ServerSceneP move_param;
    move_param.src_node_ = param.cransh_server_;
    move_param.dest_node_ = param.replace_server_;
    MoveServerScene2ServerScene(move_param);
    tls.registry.destroy(move_param.src_node_);
}

