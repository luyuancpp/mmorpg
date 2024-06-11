#include "src/system/scene/scene_system.h"

#include <ranges>

#include "muduo/base/Logging.h"

#include "src/comp/scene_comp.h"
#include "src/constants/server_constants.h"
#include "src/thread_local/thread_local_storage.h"
#include "src/constants/tips_id.h"

#include "component_proto/gs_node_comp.pb.h"
#include "event_proto/scene_event.pb.h"

static constexpr std::size_t kMaxMainScenePlayer = 1000;

using GameNodePlayerInfoPtr = std::shared_ptr<GameNodeInfo>;

void set_server_sequence_node_id(uint32_t node_id) { ScenesSystem::set_server_sequence_node_id(node_id); }

void AddMainSceneNodeComponent(const entt::entity node)
{
	tls.registry.emplace<MainSceneServer>(node);
	tls.registry.emplace<ServerComp>(node);
	tls.registry.emplace<GameNodePlayerInfoPtr>(node, std::make_shared<GameNodePlayerInfoPtr::element_type>());
}

ScenesSystem::ScenesSystem()
{
	tls.scene_registry.clear();
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
		scene_size += server_comp.GetSceneListByConfig(scene_config_id).size();
	}
	return scene_size;
}

std::size_t ScenesSystem::scenes_size()
{
	return tls.scene_registry.storage<SceneInfo>().size();
}

bool ScenesSystem::IsSceneEmpty()
{
    return tls.scene_registry.storage<SceneInfo>().empty();
}

bool ScenesSystem::ConfigSceneListNotEmpty(const uint32_t scene_config_id)
{
	for (const auto node_eid : tls.registry.view<ServerComp>())
	{
		if (auto& server_comp = tls.registry.get<ServerComp>(node_eid);
			!server_comp.GetSceneListByConfig(scene_config_id).empty())
		{
			return true;
		}
	}
	return false;
}

entt::entity ScenesSystem::CreateScene2GameNode(const CreateGameNodeSceneParam& param)
{
	if (param.IsNull())
	{
		LOG_ERROR << "server id error" << param.scene_info.scene_confid();
		return entt::null;
	}

	SceneInfo scene_info(param.scene_info);
	if (scene_info.guid() <= 0)
	{
		auto scene_id = node_sequence_.Generate();
		while (tls.scene_registry.valid(entt::entity{ scene_id }))
		{
            scene_id = node_sequence_.Generate();
		}
		scene_info.set_guid(scene_id);
	}
	auto id = entt::entity{ scene_info.guid() };
	const auto scene = tls.scene_registry.create(id);
	if (scene != id)
	{
        LOG_ERROR << "scene_registry create erroe" << scene_info.scene_confid();
        return entt::null;
	}
	tls.scene_registry.emplace<SceneInfo>(scene, scene_info);
	tls.scene_registry.emplace<ScenePlayers>(scene);

	if (auto* server_player_info = tls.registry.try_get<GameNodePlayerInfoPtr>(param.node_))
	{
		tls.scene_registry.emplace<GameNodePlayerInfoPtr>(scene, *server_player_info);
	}

	if (auto* p_server_comp = tls.registry.try_get<ServerComp>(param.node_);
		nullptr != p_server_comp)
	{
		p_server_comp->AddScene(scene);
	}

	return scene;
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
	for (const auto& conf_id_scene_list = tls.registry.get<ServerComp>(node).GetSceneList();
		auto val : conf_id_scene_list | std::views::values)
	{
		for (const auto scene : val)
		{
			DestroyScene(node, scene);
		}
	}
	Destroy(tls.registry, node);
}

uint32_t ScenesSystem::CheckScenePlayerSize(const entt::entity scene)
{
	//todo weak ptr ?
	if (tls.scene_registry.get<ScenePlayers>(scene).size() >= kMaxMainScenePlayer)
	{
		return kRetEnterSceneNotFull;
	}
	const auto* const gs_player_info = tls.scene_registry.try_get<GameNodePlayerInfoPtr>(scene);
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
	before_enter_scene_event.set_entity(entt::to_integral(param.player_));
	tls.dispatcher.trigger(before_enter_scene_event);

	tls.scene_registry.get<ScenePlayers>(param.scene_).emplace(param.player_);
	tls.registry.emplace<SceneEntity>(param.player_, param.scene_);
	// todo weak_ptr ?
	if (const auto* const game_player_info = tls.scene_registry.try_get<GameNodePlayerInfoPtr>(param.scene_))
	{
		(*game_player_info)->set_player_size((*game_player_info)->player_size() + 1);
	}

	OnEnterScene on_enter_scene_event;
	on_enter_scene_event.set_entity(entt::to_integral(param.player_));
	tls.dispatcher.trigger(on_enter_scene_event);
}

void ScenesSystem::EnterDefaultScene(const EnterDefaultSceneParam& param)
{
	if (param.IsNull())
	{
		LOG_INFO << "param null error";
		return;
	}
	const auto default_scene = NodeSceneSystem::GetNotFullScene({});
	EnterScene({default_scene, param.player_});
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

	tls.scene_registry.get<ScenePlayers>(scene).erase(param.leaver_);
	tls.registry.remove<SceneEntity>(param.leaver_);

	if (const auto* const game_player_info = tls.scene_registry.try_get<GameNodePlayerInfoPtr>(scene))
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
		CreateGameNodeSceneParam p{ .node_ = param.dest_node_ };
		p.scene_info.set_scene_confid(param.scene_conf_id_);
		scene_entity = CreateScene2GameNode(p);
	}
	LeaveScene({param.player_});
	if (entt::null == scene_entity)
	{
		EnterDefaultScene({param.player_});
		return;
	}
	EnterScene({scene_entity, param.player_});
}

void ScenesSystem::ReplaceCrashServer(entt::entity crash_node, entt::entity dest_node)
{
	for (const auto& scene_list : tls.registry.get<ServerComp>(crash_node).GetSceneList() |
		std::views::values)
	{
		for (const auto scene : scene_list)
		{
			const auto* p_scene_info = tls.scene_registry.try_get<SceneInfo>(scene);
			if (nullptr == p_scene_info)
			{
				continue;
			}
			CreateGameNodeSceneParam p{ .node_ = dest_node };
			p.scene_info.set_scene_confid(p_scene_info->scene_confid());
			CreateScene2GameNode(p);
		}
	}
	Destroy(tls.registry, crash_node);
}

