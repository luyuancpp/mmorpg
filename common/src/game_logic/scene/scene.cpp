#include "src/game_logic/scene/scene.h"

#include "muduo/base/Logging.h"

#include "src/game_logic/comp/scene_comp.h"
#include "src/game_logic/constants/server_constants.h"
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

entt::entity ScenesSystem::GetSceneByGuid(Guid guid)
{
	for (const auto server_entity : tls.registry.view<ServerComp>())
	{
		auto& server_comp = tls.registry.get<ServerComp>(server_entity);
		const entt::entity scene = server_comp.GetScenesListByGuid(guid);
		if (scene != entt::null)
		{
			return scene;
		}
	}
	return entt::null;
}

bool ScenesSystem::ConfigSceneListNotEmpty(const uint32_t scene_config_id)
{
	for (const auto server_entity : tls.registry.view<ServerComp>())
	{
		if (auto& server_comp = tls.registry.get<ServerComp>(server_entity);
			!server_comp.ConfigSceneListEmpty(scene_config_id))
		{
			return true;
		}
	}
	return false;
}

entt::entity ScenesSystem::CreateScene2Gs(const CreateGsSceneParam& param)
{
	if (param.IsNull())
	{
		LOG_ERROR << "server id error" << param.scene_confid_;
		return entt::null;
	}

	SceneInfo scene_info(param.scene_info);
	scene_info.set_scene_confid(param.scene_confid_);
	if (scene_info.scene_id() <= 0)
	{
		scene_info.set_scene_id(server_sequence_.Generate());
	}
	const auto scene_entity = tls.registry.create();
	tls.registry.emplace<SceneInfo>(scene_entity, scene_info);
	tls.registry.emplace<ScenePlayers>(scene_entity);

	if (auto* try_server_player_info = tls.registry.try_get<GsNodePlayerInfoPtr>(param.node_))
	{
		tls.registry.emplace<GsNodePlayerInfoPtr>(scene_entity, *try_server_player_info);
	}

	auto& server_scenes = tls.registry.get<ServerComp>(param.node_);
	server_scenes.AddScene(scene_entity);

	return scene_entity;
}

void ScenesSystem::DestroyScene(entt::entity node, entt::entity scene)
{
	const auto* p_scene_info = tls.registry.try_get<SceneInfo>(scene);
	if (nullptr == p_scene_info)
	{
		return;
	}
	// todo 人得换场景
	if (auto* p_server_comp = tls.registry.try_get<ServerComp>(node);
		nullptr != p_server_comp)
	{
		p_server_comp->RemoveScene(scene);
	}

	tls.registry.destroy(scene);
}

void ScenesSystem::OnDestroyServer(entt::entity node)
{
	// todo 人得换场景
	//需要拷贝，否则迭代器失效
	auto scene_list = tls.registry.get<ServerComp>(node).GetScenesList();
	for (const auto scene : scene_list | std::views::values)
	{
		DestroyScene(node, scene);
	}

	tls.registry.destroy(node);
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

void ScenesSystem::CompelPlayerChangeScene(const CompelChangeSceneParam& param)
{
	const auto& dest_node_scene = tls.registry.get<ServerComp>(param.dest_node_);
	entt::entity scene_entity = dest_node_scene.GetMinPlayerSizeSceneByConfigId(param.scene_conf_id_);
	if (entt::null == scene_entity)
	{
		CreateGsSceneParam create_gs_scene_param;
		create_gs_scene_param.scene_confid_ = param.scene_conf_id_;
		create_gs_scene_param.node_ = param.dest_node_;
		scene_entity = CreateScene2Gs(create_gs_scene_param);
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

void ScenesSystem::ReplaceCrashServer(entt::entity crash_node, entt::entity dest_node)
{
	auto& src_server_scene_list = tls.registry.get<ServerComp>(crash_node).GetScenesList();
	for (const auto& scene : src_server_scene_list)
	{
		auto* p_scene_info = tls.registry.try_get<SceneInfo>(scene.second);
		if (nullptr == p_scene_info)
		{
			continue;
		}
		CreateGsSceneParam create_gs_scene_param;
		create_gs_scene_param.scene_confid_ = p_scene_info->scene_confid();
		create_gs_scene_param.node_ = dest_node;
		CreateScene2Gs(create_gs_scene_param);
	}

	tls.registry.destroy(crash_node);
}

