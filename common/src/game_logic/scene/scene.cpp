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
	for (const auto server_entity : tls.registry.view<ServerComp>())
	{
		tls.registry.destroy(server_entity);
	}
}

std::size_t ScenesSystem::scenes_size(uint32_t scene_config_id)
{
	std::size_t scene_size = 0;
	for (const auto server_entity : tls.registry.view<ServerComp>())
	{
		auto& server_comp = tls.registry.get<ServerComp>(server_entity);
		scene_size += server_comp.ConfigSceneSize(scene_config_id);
	}
	return scene_size;
}

std::size_t ScenesSystem::scenes_size()
{
	std::size_t scene_size = 0;
	for (const auto server_entity : tls.registry.view<ServerComp>())
	{
		auto& server_comp = tls.registry.get<ServerComp>(server_entity);
		scene_size += server_comp.GetSceneSize();
	}
	return scene_size;
}

bool ScenesSystem::IsSceneEmpty()
{
	for (const auto server_entity : tls.registry.view<ServerComp>())
	{
		if (auto& server_comp = tls.registry.get<ServerComp>(server_entity);
			!server_comp.IsSceneEmpty())
		{
			return false;
		}
	}
	return true;
}

bool ScenesSystem::HasConfigScene(const uint32_t scene_config_id)
{
	for (const auto server_entity : tls.registry.view<ServerComp>())
	{
		if (auto& server_comp = tls.registry.get<ServerComp>(server_entity);
			server_comp.HasConfigScene(scene_config_id))
		{
			return true;
		}
	}
	return false;
}

entt::entity ScenesSystem::CreateSceneByGuid(const CreateSceneBySceneInfoP& param)
{
	const auto entity = tls.registry.create();
	tls.registry.emplace<SceneInfo>(entity, std::move(param.scene_info_));
	tls.registry.emplace<ScenePlayers>(entity);
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
	const auto& scene_info = tls.registry.get<SceneInfo>(param.scene_);
	auto& server_scene = tls.registry.get<ServerComp>(param.node_);
	server_scene.RemoveScene(scene_info.scene_confid(), param.scene_);
	
	tls.registry.destroy(param.scene_);
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
	for (const auto& val : tls.registry.get<ServerComp>(param.node_).GetConfidScenesList() | std::views::values)
	{
		for (const auto& ji : val)
		{
			server_scenes_set.emplace(ji);
		}
	}
	DestroySceneParam destroy_scene_param;
	destroy_scene_param.node_ = param.node_;
	for (auto& it : server_scenes_set)
	{
		destroy_scene_param.scene_ = it;
		DestroyScene(destroy_scene_param);
	}
	tls.registry.destroy(param.node_);
}

uint32_t ScenesSystem::CheckScenePlayerSize(entt::entity scene)
{
	//todo weak ptr ?
	if (tls.registry.get<ScenePlayers>(scene).size() >= kMaxMainScenePlayer)
	{
		return kRetEnterSceneNotFull;
	}
	const auto* const try_gs_player_info = tls.registry.try_get<GsNodePlayerInfoPtr>(scene);
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

	BeforeEnterScene before_enter_scene_event;
	before_enter_scene_event.set_entity(entt::to_integral(param.enterer_));
	tls.dispatcher.trigger(before_enter_scene_event);

	tls.registry.get<ScenePlayers>(param.scene_).emplace(param.enterer_);
	tls.registry.emplace<SceneEntity>(param.enterer_, param.scene_);
	// todo weak_ptr ?
	if (const auto* const try_gs_player_info = tls.registry.try_get<GsNodePlayerInfoPtr>(param.scene_))
	{
		(*try_gs_player_info)->set_player_size((*try_gs_player_info)->player_size() + 1);
	}

	OnEnterScene on_enter_scene_event;
	on_enter_scene_event.set_entity(entt::to_integral(param.enterer_));
	tls.dispatcher.trigger(on_enter_scene_event);

}

void ScenesSystem::LeaveScene(const LeaveSceneParam& param)
{
	if (param.IsNull())
	{
		LOG_ERROR << "entity null";
		return;
	}
	if (nullptr == tls.registry.try_get<SceneEntity>(param.leaver_))
	{
		LOG_ERROR << "leave scene empty";
		return;
	}

	const auto scene = tls.registry.get<SceneEntity>(param.leaver_).scene_entity_;

	BeforeLeaveScene before_leave_scene_event;
	before_leave_scene_event.set_entity(entt::to_integral(param.leaver_));
	tls.dispatcher.trigger(before_leave_scene_event);

	tls.registry.get<ScenePlayers>(scene).erase(param.leaver_);
	tls.registry.remove<SceneEntity>(param.leaver_);

	if (const auto* const try_gs_player_info = tls.registry.try_get<GsNodePlayerInfoPtr>(scene))
	{
		(*try_gs_player_info)->set_player_size((*try_gs_player_info)->player_size() - 1);
	}

	OnLeaveScene on_leave_scene_event;
	on_leave_scene_event.set_entity(entt::to_integral(param.leaver_));
	tls.dispatcher.trigger(on_leave_scene_event);
}

void ScenesSystem::CompelToChangeScene(const CompelChangeSceneParam& param)
{
	const auto& new_server_scene = tls.registry.get<ServerComp>(param.new_server_);
	entt::entity scene_entity{entt::null};
	if (!new_server_scene.HasConfig(param.scene_confid_))
	{
		CreateGsSceneP create_gs_scene_param;
		create_gs_scene_param.scene_confid_ = param.scene_confid_;
		create_gs_scene_param.node_ = param.new_server_;
		 scene_entity = CreateScene2Gs(create_gs_scene_param);
	}
	else
	{
		//todo 第一个 场景压力会特别大
		 scene_entity = new_server_scene.GetFirstSceneByConfigId(param.scene_confid_);
	}

	if (entt::null == scene_entity)
	{
		//todo 回到默认场景
		return;
	}

	LeaveSceneParam leave_param;
	leave_param.leaver_ = param.player_;
	LeaveScene(leave_param);

	EnterSceneParam enter_param;
	enter_param.enterer_ = param.player_;
	enter_param.scene_ = scene_entity;
	EnterScene(enter_param);
}

void ScenesSystem::ReplaceCrashServer(const ReplaceCrashServerParam& param)
{
	if (param.IsNull())
	{
		return;
	}
	auto& dest_server_data = tls.registry.get<GsNodePlayerInfoPtr>(param.replace_server_);
	auto& dest_server_comp = tls.registry.get<ServerComp>(param.replace_server_);
	for (const auto& src_server_scene = tls.registry.get<ServerComp>(param.cransh_server_).GetConfidScenesList();
		const auto& [fst, snd] : src_server_scene)
	{
		for (const auto& ji : snd)
		{
			CreateGsSceneP create_gs_scene_param;
			create_gs_scene_param.scene_confid_ = fst;
			create_gs_scene_param.node_ = param.replace_server_;
			CreateScene2Gs(create_gs_scene_param);
		}
	}

	tls.registry.destroy(param.cransh_server_);
}

