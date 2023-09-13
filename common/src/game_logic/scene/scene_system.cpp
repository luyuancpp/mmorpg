#include "src/game_logic/scene/scene_system.h"

#include <ranges>

#include "muduo/base/Logging.h"

#include "src/game_logic/comp/scene_comp.h"
#include "src/game_logic/constants/server_constants.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/game_logic/tips_id.h"

#include "component_proto/gs_node_comp.pb.h"
#include "event_proto/scene_event.pb.h"

static constexpr std::size_t kMaxMainScenePlayer = 1000;

using GameNodePlayerInfoPtr = std::shared_ptr<GsNodePlayerInfo>;

void set_server_sequence_node_id(uint32_t node_id) { ScenesSystem::set_server_sequence_node_id(node_id); }

void AddMainSceneNodeComponent(const entt::entity server)
{
	tls.registry.emplace<MainSceneServer>(server);
	tls.registry.emplace<ServerComp>(server);
	tls.registry.emplace<GameNodePlayerInfoPtr>(server, std::make_shared<GameNodePlayerInfoPtr::element_type>());
}

ScenesSystem::ScenesSystem()
{
	cl_tls.scene_list().clear();
}

ScenesSystem::~ScenesSystem()
{
	tls.ClearForTest();
}

std::size_t ScenesSystem::scenes_size(uint32_t scene_config_id)
{
	std::size_t scene_size = 0;
	for (const auto server_entity : tls.registry.view<ServerComp>())
	{
		auto& server_comp = tls.registry.get<ServerComp>(server_entity);
		scene_size += server_comp.GetScenesListByConfig(scene_config_id).size();
	}
	return scene_size;
}

std::size_t ScenesSystem::scenes_size()
{
	return cl_tls.scene_list().size();
}

bool ScenesSystem::IsSceneEmpty()
{
	return cl_tls.scene_list().empty();
}

entt::entity ScenesSystem::GetSceneByGuid(Guid guid)
{
	const auto scene_it = cl_tls.scene_list().find(guid);
	if (scene_it == cl_tls.scene_list().end())
	{
		return entt::null;
	}
	return scene_it->second;
}

bool ScenesSystem::ConfigSceneListNotEmpty(const uint32_t scene_config_id)
{
	for (const auto server_entity : tls.registry.view<ServerComp>())
	{
		if (auto& server_comp = tls.registry.get<ServerComp>(server_entity);
			!server_comp.GetScenesListByConfig(scene_config_id).empty())
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
	if (scene_info.guid() <= 0)
	{
		scene_info.set_guid(server_sequence_.Generate());
	}
	const auto scene_entity = tls.registry.create();
	tls.registry.emplace<SceneInfo>(scene_entity, scene_info);
	tls.registry.emplace<ScenePlayers>(scene_entity);

	if (auto* server_player_info = tls.registry.try_get<GameNodePlayerInfoPtr>(param.node_))
	{
		tls.registry.emplace<GameNodePlayerInfoPtr>(scene_entity, *server_player_info);
	}

	auto* p_server_comp = tls.registry.try_get<ServerComp>(param.node_);
	if (nullptr != p_server_comp)
	{
		p_server_comp->AddScene(scene_entity);
	}

	return scene_entity;
}

void ScenesSystem::DestroyScene(entt::entity node, entt::entity scene)
{
	// todo 人得换场景
	auto* p_server_comp = tls.registry.try_get<ServerComp>(node);
	if (nullptr == p_server_comp)
	{
		return;
	}
	p_server_comp->RemoveScene(scene);
}

void ScenesSystem::OnDestroyServer(entt::entity node)
{
	// todo 人得换场景
	//需要拷贝，否则迭代器失效
	for (const auto& conf_id_scene_list = tls.registry.get<ServerComp>(node).GetScenesList();
		auto val : conf_id_scene_list | std::views::values)
	{
		for (const auto scene : val | std::views::values)
		{
			DestroyScene(node, scene);
		}
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
	const auto* const gs_player_info = tls.registry.try_get<GameNodePlayerInfoPtr>(scene);
	if (nullptr == gs_player_info)
	{
		LOG_ERROR << " gs null";
		return kRetEnterSceneGsInfoNull;
	}
	if ((*gs_player_info)->player_size() >= kMaxServerPlayerSize)
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
	if (const auto* const game_player_info = tls.registry.try_get<GameNodePlayerInfoPtr>(param.scene_))
	{
		(*game_player_info)->set_player_size((*game_player_info)->player_size() + 1);
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

	if (const auto* const game_player_info = tls.registry.try_get<GameNodePlayerInfoPtr>(scene))
	{
		(*game_player_info)->set_player_size((*game_player_info)->player_size() - 1);
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
	for (const auto& scene_list : tls.registry.get<ServerComp>(crash_node).GetScenesList() |
		std::views::values)
	{
		for (const auto scene : scene_list | std::views::values)
		{
			const auto* p_scene_info = tls.registry.try_get<SceneInfo>(scene);
			if (nullptr == p_scene_info)
			{
				continue;
			}
			CreateGsSceneParam create_gs_scene_param;
			create_gs_scene_param.scene_confid_ = p_scene_info->scene_confid();
			create_gs_scene_param.node_ = dest_node;
			CreateScene2Gs(create_gs_scene_param);
		}
	}

	tls.registry.destroy(crash_node);
}

