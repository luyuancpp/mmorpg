#include "system/scene/scene_system.h"

#include <ranges>

#include "muduo/base/Logging.h"

#include "comp/scene.h"
#include "constants/node.h"
#include "thread_local/storage.h"
#include "constants/tips_id.h"

#include "proto/logic/component/gs_node_comp.pb.h"
#include "proto/logic/event/scene_event.pb.h"

static constexpr std::size_t kMaxScenePlayer = 1000;

using GameNodePlayerInfoPtr = std::shared_ptr<GameNodeInfo>;

void set_server_sequence_node_id(uint32_t node_id) { ScenesSystem::set_sequence_node_id(node_id); }

void AddMainSceneNodeComponent(entt::registry& reg, const entt::entity node)
{
	reg.emplace<MainSceneServer>(node);
	reg.emplace<ServerComp>(node);
	reg.emplace<GameNodePlayerInfoPtr>(node, std::make_shared<GameNodePlayerInfoPtr::element_type>());
}

ScenesSystem::ScenesSystem()
{
	tls.scene_registry.clear();
	tls.registry.clear();
	tls.game_node_registry.clear();
}

ScenesSystem::~ScenesSystem()
{
	tls.ClearForTest();
}

NodeId ScenesSystem::get_game_node_id(entt::entity scene)
{
    auto scene_info = tls.scene_registry.try_get<SceneInfo>(scene);
    if (nullptr == scene_info)
    {
        return kInvalidNodeId;
    }
	return get_game_node_id(scene_info->guid());
}

uint32_t ScenesSystem::GenSceneGuid()
{
    auto scene_id = node_sequence_.Generate();
    while (tls.scene_registry.valid(entt::entity{ scene_id }))
    {
        scene_id = node_sequence_.Generate();
    }
	return scene_id;
}

std::size_t ScenesSystem::scenes_size(uint32_t scene_config_id)
{
	std::size_t scene_size = 0;
	for (const auto node : tls.game_node_registry.view<ServerComp>())
	{
		auto& server_comp = tls.game_node_registry.get<ServerComp>(node);
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
	for (const auto node_eid : tls.game_node_registry.view<ServerComp>())
	{
		if (auto& server_comp = tls.game_node_registry.get<ServerComp>(node_eid);
			!server_comp.GetSceneListByConfig(scene_config_id).empty())
		{
			return true;
		}
	}
	return false;
}

entt::entity ScenesSystem::CreateScene2GameNode(const CreateGameNodeSceneParam& param)
{
	if (param.CheckValid())
	{
		LOG_ERROR << "server id error" << param.scene_info.scene_confid();
		return entt::null;
	}

	SceneInfo scene_info(param.scene_info);
	if (scene_info.guid() <= 0)
	{
		scene_info.set_guid(GenSceneGuid());
	}
	const auto id = entt::entity{ scene_info.guid() };
	const auto scene = tls.scene_registry.create(id);
	if (scene != id)
	{
        LOG_ERROR << "scene_registry create erroe" << scene_info.scene_confid();
        return entt::null;
	}
	tls.scene_registry.emplace<SceneInfo>(scene, std::move(scene_info));
	tls.scene_registry.emplace<ScenePlayers>(scene);

	if (auto* server_player_info
		= tls.game_node_registry.try_get<GameNodePlayerInfoPtr>(param.node))
	{
		tls.scene_registry.emplace<GameNodePlayerInfoPtr>(scene, *server_player_info);
	}

	if (auto* p_server_comp = tls.game_node_registry.try_get<ServerComp>(param.node);
		nullptr != p_server_comp)
	{
		p_server_comp->AddScene(scene);
	}
	
	OnSceneCreate create_scene_event;
	create_scene_event.set_entity(entt::to_integral(scene));
	tls.dispatcher.trigger(create_scene_event);
	return scene;
}

void ScenesSystem::DestroyScene(const DestroySceneParam& param)
{
	if (param.CheckValid())
	{
		return;
	}
	// todo 人得换场景
	auto* p_server_comp = tls.game_node_registry.try_get<ServerComp>(param.node);
	if (nullptr == p_server_comp)
	{
		return;
	}
	
	OnDestroyScene destroy_scene_event;
	destroy_scene_event.set_entity(entt::to_integral(param.scene));
	tls.dispatcher.trigger(destroy_scene_event);
	
	p_server_comp->RemoveScene(param.scene);
}

void ScenesSystem::OnDestroyServer(entt::entity node)
{
	// todo 人得换场景
	//需要拷贝，否则迭代器失效
	for (const auto& conf_id_scene_list = tls.game_node_registry.get<ServerComp>(node).GetSceneList();
		auto val : conf_id_scene_list | std::views::values)
	{
		for (const auto scene : val)
		{
			DestroyScene({ node, scene });
		}
	}
	Destroy(tls.game_node_registry, node);
}

uint32_t ScenesSystem::CheckEnterScene(const EnterSceneParam& param)
{
	if (!tls.scene_registry.valid(param.scene))
	{
		return kRetCheckEnterSceneSceneParam;
	}
	auto scene_info = tls.scene_registry.try_get<SceneInfo>(param.scene);
	if (nullptr == scene_info)
	{
        return kRetCheckEnterSceneSceneParam;
	}
	if (scene_info->creators().find(tls.registry.get<Guid>(param.enter)) == 
		scene_info->creators().end())
	{
		return kRetCheckEnterSceneCreator;
	}
	return kOK;
}

uint32_t ScenesSystem::CheckScenePlayerSize(const entt::entity scene)
{
	//todo weak ptr ?
	if (tls.scene_registry.get<ScenePlayers>(scene).size() >= kMaxScenePlayer)
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
	return kOK;
}

void ScenesSystem::EnterScene(const EnterSceneParam& param)
{
	if (param.CheckValid())
	{
		LOG_ERROR << "param null error";
		return;
	}

	BeforeEnterScene before_enter_scene_event;
	before_enter_scene_event.set_entity(entt::to_integral(param.enter));
	tls.dispatcher.trigger(before_enter_scene_event);

	tls.scene_registry.get<ScenePlayers>(param.scene).emplace(param.enter);
	tls.registry.emplace<SceneEntity>(param.enter, param.scene);
	// todo weak_ptr ?
	if (const auto* const game_player_info = tls.scene_registry.try_get<GameNodePlayerInfoPtr>(param.scene))
	{
		(*game_player_info)->set_player_size((*game_player_info)->player_size() + 1);
	}

	AfterEnterScene on_enter_scene_event;
	on_enter_scene_event.set_entity(entt::to_integral(param.enter));
	tls.dispatcher.trigger(on_enter_scene_event);
}

void ScenesSystem::EnterDefaultScene(const EnterDefaultSceneParam& param)
{
	if (param.CheckValid())
	{
		LOG_ERROR << "param null error";
		return;
	}
	const auto default_scene = NodeSceneSystem::GetNotFullScene({});
	EnterScene({default_scene, param.enter});
}

void ScenesSystem::LeaveScene(const LeaveSceneParam& param)
{
	if (param.CheckValid())
	{
		LOG_ERROR << "entity null";
		return;
	}
	if (nullptr == tls.registry.try_get<SceneEntity>(param.leaver))
	{
		LOG_ERROR << "leave scene empty";
		return;
	}

	const auto scene = tls.registry.get<SceneEntity>(param.leaver).scene_entity;

	BeforeLeaveScene before_leave_scene_event;
	before_leave_scene_event.set_entity(entt::to_integral(param.leaver));
	tls.dispatcher.trigger(before_leave_scene_event);

	tls.scene_registry.get<ScenePlayers>(scene).erase(param.leaver);
	tls.registry.remove<SceneEntity>(param.leaver);

	if (const auto* const game_player_info = tls.scene_registry.try_get<GameNodePlayerInfoPtr>(scene))
	{
		(*game_player_info)->set_player_size((*game_player_info)->player_size() - 1);
	}

	AfterLeaveScene on_leave_scene_event;
	on_leave_scene_event.set_entity(entt::to_integral(param.leaver));
	tls.dispatcher.trigger(on_leave_scene_event);
}

void ScenesSystem::CompelPlayerChangeScene(const CompelChangeSceneParam& param)
{
	const auto& dest_node_scene = tls.game_node_registry.get<ServerComp>(param.dest_node_);
	entt::entity scene_entity = dest_node_scene.GetMinPlayerSizeSceneByConfigId(param.scene_conf_id_);
	if (entt::null == scene_entity)
	{
		CreateGameNodeSceneParam p{ .node = param.dest_node_ };
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
	for (const auto& scene_list : tls.game_node_registry.get<ServerComp>(crash_node).GetSceneList() |
		std::views::values)
	{
		for (const auto scene : scene_list)
		{
			const auto* p_scene_info = tls.scene_registry.try_get<SceneInfo>(scene);
			if (nullptr == p_scene_info)
			{
				continue;
			}
			CreateGameNodeSceneParam p{ .node = dest_node };
			p.scene_info.set_scene_confid(p_scene_info->scene_confid());
			CreateScene2GameNode(p);
		}
	}
	Destroy(tls.game_node_registry, crash_node);
}

