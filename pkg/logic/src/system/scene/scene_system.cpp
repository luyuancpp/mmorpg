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

void SetServerSequenceNodeId(uint32_t nodeId) { ScenesSystem::SetSequenceNodeId(nodeId); }

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

NodeId ScenesSystem::GetGameNodeId(entt::entity scene)
{
	auto sceneInfo = tls.scene_registry.try_get<SceneInfo>(scene);
	if (nullptr == sceneInfo)
	{
		return kInvalidNodeId;
	}
	return GetGameNodeId(sceneInfo->guid());
}

uint32_t ScenesSystem::GenSceneGuid()
{
	auto sceneId = nodeSequence.Generate();
	while (tls.scene_registry.valid(entt::entity{ sceneId }))
	{
		sceneId = nodeSequence.Generate();
	}
	return sceneId;
}

std::size_t ScenesSystem::GetScenesSize(uint32_t sceneConfigId)
{
	std::size_t sceneSize = 0;
	for (const auto node : tls.game_node_registry.view<ServerComp>())
	{
		auto& serverComp = tls.game_node_registry.get<ServerComp>(node);
		sceneSize += serverComp.GetSceneListByConfig(sceneConfigId).size();
	}
	return sceneSize;
}

std::size_t ScenesSystem::GetScenesSize()
{
	return tls.scene_registry.storage<SceneInfo>().size();
}

bool ScenesSystem::IsSceneEmpty()
{
	return tls.scene_registry.storage<SceneInfo>().empty();
}

bool ScenesSystem::ConfigSceneListNotEmpty(const uint32_t sceneConfigId)
{
	for (const auto nodeEid : tls.game_node_registry.view<ServerComp>())
	{
		if (auto& serverComp = tls.game_node_registry.get<ServerComp>(nodeEid);
			!serverComp.GetSceneListByConfig(sceneConfigId).empty())
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
		LOG_ERROR << "server id error" << param.sceneInfo.scene_confid();
		return entt::null;
	}

	SceneInfo sceneInfo(param.sceneInfo);
	if (sceneInfo.guid() <= 0)
	{
		sceneInfo.set_guid(GenSceneGuid());
	}
	const auto id = entt::entity{ sceneInfo.guid() };
	const auto scene = tls.scene_registry.create(id);
	if (scene != id)
	{
		LOG_ERROR << "scene_registry create error" << sceneInfo.scene_confid();
		return entt::null;
	}
	tls.scene_registry.emplace<SceneInfo>(scene, std::move(sceneInfo));
	tls.scene_registry.emplace<ScenePlayers>(scene);

	if (auto* serverPlayerInfo
		= tls.game_node_registry.try_get<GameNodePlayerInfoPtr>(param.node))
	{
		tls.scene_registry.emplace<GameNodePlayerInfoPtr>(scene, *serverPlayerInfo);
	}

	if (auto* pServerComp = tls.game_node_registry.try_get<ServerComp>(param.node);
		nullptr != pServerComp)
	{
		pServerComp->AddScene(scene);
	}

	OnSceneCreate createSceneEvent;
	createSceneEvent.set_entity(entt::to_integral(scene));
	tls.dispatcher.trigger(createSceneEvent);
	return scene;
}

void ScenesSystem::DestroyScene(const DestroySceneParam& param)
{
	if (param.CheckValid())
	{
		return;
	}
	// todo 人得换场景
	auto* pServerComp = tls.game_node_registry.try_get<ServerComp>(param.node);
	if (nullptr == pServerComp)
	{
		return;
	}

	OnDestroyScene destroySceneEvent;
	destroySceneEvent.set_entity(entt::to_integral(param.scene));
	tls.dispatcher.trigger(destroySceneEvent);

	pServerComp->RemoveScene(param.scene);
}

void ScenesSystem::OnDestroyServer(entt::entity node)
{
	// todo 人得换场景
	// 需要拷贝，否则迭代器失效
	for (const auto& confIdSceneList = tls.game_node_registry.get<ServerComp>(node).GetSceneList();
		auto val : confIdSceneList | std::views::values)
	{
		for (const auto scene : val)
		{
			DestroyScene({ node, scene });
		}
	}
	Destroy(tls.game_node_registry, node);
}

uint32_t ScenesSystem::CheckPlayerEnterScene(const EnterSceneParam& param)
{
	if (!tls.scene_registry.valid(param.scene))
	{
		return kRetCheckEnterSceneSceneParam;
	}
	auto sceneInfo = tls.scene_registry.try_get<SceneInfo>(param.scene);
	if (nullptr == sceneInfo)
	{
		return kRetCheckEnterSceneSceneParam;
	}
	if (sceneInfo->creators().find(tls.registry.get<Guid>(param.enter)) ==
		sceneInfo->creators().end())
	{
		return kRetCheckEnterSceneCreator;
	}
	return kOK;
}

uint32_t ScenesSystem::CheckScenePlayerSize(const entt::entity scene)
{
	// todo weak ptr ?
	if (tls.scene_registry.get<ScenePlayers>(scene).size() >= kMaxScenePlayer)
	{
		return kRetEnterSceneNotFull;
	}
	const auto* const gsPlayerInfo = tls.scene_registry.try_get<GameNodePlayerInfoPtr>(scene);
	if (nullptr == gsPlayerInfo)
	{
		LOG_ERROR << " gs null";
		return kRetEnterSceneGsInfoNull;
	}
	if ((*gsPlayerInfo)->player_size() >= kMaxServerPlayerSize)
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

	BeforeEnterScene beforeEnterSceneEvent;
	beforeEnterSceneEvent.set_entity(entt::to_integral(param.enter));
	tls.dispatcher.trigger(beforeEnterSceneEvent);

	tls.scene_registry.get<ScenePlayers>(param.scene).emplace(param.enter);
	tls.registry.emplace<SceneEntity>(param.enter, param.scene);
	// todo weak_ptr ?
	if (const auto* const gamePlayerInfo = tls.scene_registry.try_get<GameNodePlayerInfoPtr>(param.scene))
	{
		(*gamePlayerInfo)->set_player_size((*gamePlayerInfo)->player_size() + 1);
	}

	AfterEnterScene onEnterSceneEvent;
	onEnterSceneEvent.set_entity(entt::to_integral(param.enter));
	tls.dispatcher.trigger(onEnterSceneEvent);
}

void ScenesSystem::EnterDefaultScene(const EnterDefaultSceneParam& param)
{
	if (param.CheckValid())
	{
		LOG_ERROR << "param null error";
		return;
	}
	const auto defaultScene = NodeSceneSystem::GetNotFullScene({});
	EnterScene({ defaultScene, param.enter });
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

	const auto scene = tls.registry.get<SceneEntity>(param.leaver).sceneEntity;

	BeforeLeaveScene beforeLeaveSceneEvent;
	beforeLeaveSceneEvent.set_entity(entt::to_integral(param.leaver));
	tls.dispatcher.trigger(beforeLeaveSceneEvent);

	tls.scene_registry.get<ScenePlayers>(scene).erase(param.leaver);
	tls.registry.remove<SceneEntity>(param.leaver);

	if (const auto* const gamePlayerInfo = tls.scene_registry.try_get<GameNodePlayerInfoPtr>(scene))
	{
		(*gamePlayerInfo)->set_player_size((*gamePlayerInfo)->player_size() - 1);
	}

	AfterLeaveScene onLeaveSceneEvent;
	onLeaveSceneEvent.set_entity(entt::to_integral(param.leaver));
	tls.dispatcher.trigger(onLeaveSceneEvent);
}

void ScenesSystem::CompelPlayerChangeScene(const CompelChangeSceneParam& param)
{
	const auto& destNodeScene = tls.game_node_registry.get<ServerComp>(param.destNode);
	entt::entity sceneEntity = destNodeScene.GetMinPlayerSizeSceneByConfigId(param.sceneConfId);
	if (entt::null == sceneEntity)
	{
		CreateGameNodeSceneParam p{ .node = param.destNode };
		p.sceneInfo.set_scene_confid(param.sceneConfId);
		sceneEntity = CreateScene2GameNode(p);
	}
	LeaveScene({ param.player_ });
	if (entt::null == sceneEntity)
	{
		EnterDefaultScene({

param.player_ });
		return;
	}
	EnterScene({ sceneEntity, param.player_ });
}

void ScenesSystem::ReplaceCrashServer(entt::entity crashNode, entt::entity destNode)
{
	for (const auto& sceneList : tls.game_node_registry.get<ServerComp>(crashNode).GetSceneList() |
		std::views::values)
	{
		for (const auto scene : sceneList)
		{
			const auto* pSceneInfo = tls.scene_registry.try_get<SceneInfo>(scene);
			if (nullptr == pSceneInfo)
			{
				continue;
			}
			CreateGameNodeSceneParam p{ .node = destNode };
			p.sceneInfo.set_scene_confid(pSceneInfo->scene_confid());
			CreateScene2GameNode(p);
		}
	}
	Destroy(tls.game_node_registry, crashNode);
}
