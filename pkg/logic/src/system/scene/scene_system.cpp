#include "system/scene/scene_system.h"
#include "muduo/base/Logging.h"

#include "comp/scene.h"
#include "constants/node.h"
#include "thread_local/storage.h"
#include "constants/tips_id.h"

#include "proto/logic/component/gs_node_comp.pb.h"
#include "proto/logic/event/scene_event.pb.h"

#include <ranges> // Only if using C++20 ranges

static constexpr std::size_t kMaxScenePlayer = 1000;

using GameNodePlayerInfoPtr = std::shared_ptr<GameNodeInfo>;

void SetServerSequenceNodeId(uint32_t nodeId) {
	ScenesSystem::SetSequenceNodeId(nodeId);
}

void AddMainSceneNodeComponent(entt::registry& reg, const entt::entity node) {
	reg.emplace<MainSceneServer>(node);
	reg.emplace<ServerComp>(node);
	reg.emplace<GameNodePlayerInfoPtr>(node, std::make_shared<GameNodeInfo>());
}

ScenesSystem::ScenesSystem() {
	Clear();
}

ScenesSystem::~ScenesSystem() {
	Clear();
}

void ScenesSystem::Clear() {
	tls.scene_registry.clear();
	tls.registry.clear();
	tls.game_node_registry.clear();
}

NodeId ScenesSystem::GetGameNodeId(entt::entity scene) {
	if (auto sceneInfo = tls.scene_registry.try_get<SceneInfo>(scene)) {
		return GetGameNodeId(sceneInfo->guid());
	}
	return kInvalidNodeId;
}

uint32_t ScenesSystem::GenSceneGuid() {
	uint32_t sceneId = nodeSequence.Generate();
	while (tls.scene_registry.valid(entt::entity{ sceneId })) {
		sceneId = nodeSequence.Generate();
	}
	return sceneId;
}

std::size_t ScenesSystem::GetScenesSize(uint32_t sceneConfigId) {
	std::size_t sceneSize = 0;
	for (auto node : tls.game_node_registry.view<ServerComp>()) {
		auto& serverComp = tls.game_node_registry.get<ServerComp>(node);
		sceneSize += serverComp.GetSceneListByConfig(sceneConfigId).size();
	}
	return sceneSize;
}

std::size_t ScenesSystem::GetScenesSize() {
	return tls.scene_registry.storage<SceneInfo>().size();
}

bool ScenesSystem::IsSceneEmpty() {
	return tls.scene_registry.storage<SceneInfo>().empty();
}

bool ScenesSystem::ConfigSceneListNotEmpty(uint32_t sceneConfigId) {
	for (auto nodeEid : tls.game_node_registry.view<ServerComp>()) {
		auto& serverComp = tls.game_node_registry.get<ServerComp>(nodeEid);
		if (!serverComp.GetSceneListByConfig(sceneConfigId).empty()) {
			return true;
		}
	}
	return false;
}

entt::entity ScenesSystem::CreateScene2GameNode(const CreateGameNodeSceneParam& param) {
	if (param.CheckValid()) {
		LOG_ERROR << "CreateScene2GameNode: Invalid parameters";
		return entt::null;
	}

	SceneInfo sceneInfo(param.sceneInfo);
	if (sceneInfo.guid() <= 0) {
		sceneInfo.set_guid(GenSceneGuid());
	}

	const auto scene = tls.scene_registry.create(entt::entity{ sceneInfo.guid() });
	if (scene == entt::null) {
		LOG_ERROR << "CreateScene2GameNode: Failed to create scene in registry";
		return entt::null;
	}

	tls.scene_registry.emplace<SceneInfo>(scene, std::move(sceneInfo));
	tls.scene_registry.emplace<ScenePlayers>(scene);

	if (auto* serverPlayerInfo = tls.game_node_registry.try_get<GameNodePlayerInfoPtr>(param.node)) {
		tls.scene_registry.emplace<GameNodePlayerInfoPtr>(scene, *serverPlayerInfo);
	}

	if (auto* pServerComp = tls.game_node_registry.try_get<ServerComp>(param.node)) {
		pServerComp->AddScene(scene);
	}

	OnSceneCreate createSceneEvent;
	createSceneEvent.set_entity(entt::to_integral(scene));
	tls.dispatcher.trigger(createSceneEvent);
	return scene;
}

void ScenesSystem::DestroyScene(const DestroySceneParam& param) {
	if (param.CheckValid()) {
		LOG_ERROR << "DestroyScene: Invalid parameters";
		return;
	}

	auto* pServerComp = tls.game_node_registry.try_get<ServerComp>(param.node);
	if (!pServerComp) {
		LOG_ERROR << "DestroyScene: ServerComp not found for node";
		return;
	}

	auto* sceneInfo = tls.scene_registry.try_get<SceneInfo>(param.scene);
	if (!sceneInfo) {
		LOG_ERROR << "DestroyScene: SceneInfo not found for scene";
		return;
	}

	OnDestroyScene destroySceneEvent;
	destroySceneEvent.set_entity(entt::to_integral(param.scene));
	tls.dispatcher.trigger(destroySceneEvent);

	pServerComp->RemoveScene(param.scene);
}

void ScenesSystem::OnDestroyServer(entt::entity node) {
	auto& serverComp = tls.game_node_registry.get<ServerComp>(node);
	auto sceneLists = serverComp.GetSceneList();

	for (auto& confIdSceneList : sceneLists | std::views::values) {
		for (auto scene : confIdSceneList) {
			DestroyScene({ node, scene });
		}
	}

	Destroy(tls.game_node_registry, node);
}

uint32_t ScenesSystem::CheckPlayerEnterScene(const EnterSceneParam& param) {
	if (!tls.scene_registry.valid(param.scene)) {
		return kRetCheckEnterSceneSceneParam;
	}

	auto* sceneInfo = tls.scene_registry.try_get<SceneInfo>(param.scene);
	if (!sceneInfo) {
		return kRetCheckEnterSceneSceneParam;
	}

	if (sceneInfo->creators().find(tls.registry.get<Guid>(param.enter)) == sceneInfo->creators().end()) {
		return kRetCheckEnterSceneCreator;
	}

	return kOK;
}

uint32_t ScenesSystem::CheckScenePlayerSize(entt::entity scene) {
	auto& scenePlayers = tls.scene_registry.get<ScenePlayers>(scene);

	if (scenePlayers.size() >= kMaxScenePlayer) {
		return kRetEnterSceneNotFull;
	}

	auto* gsPlayerInfo = tls.scene_registry.try_get<GameNodePlayerInfoPtr>(scene);
	if (!gsPlayerInfo) {
		LOG_ERROR << "CheckScenePlayerSize: GameNodePlayerInfoPtr not found for scene";
		return kRetEnterSceneGsInfoNull;
	}

	if ((*gsPlayerInfo)->player_size() >= kMaxServerPlayerSize) {
		return kRetEnterSceneGsFull;
	}

	return kOK;
}

void ScenesSystem::EnterScene(const EnterSceneParam& param) {
	if (param.CheckValid()) {
		LOG_ERROR << "EnterScene: Invalid parameters";
		return;
	}

	auto& scenePlayers = tls.scene_registry.get<ScenePlayers>(param.scene);
	scenePlayers.emplace(param.enter);
	tls.registry.emplace<SceneEntity>(param.enter, param.scene);

	auto* gsPlayerInfo = tls.scene_registry.try_get<GameNodePlayerInfoPtr>(param.scene);
	if (gsPlayerInfo) {
		(*gsPlayerInfo)->set_player_size((*gsPlayerInfo)->player_size() + 1);
	}

	AfterEnterScene afterEnterScene;
	afterEnterScene.set_entity(entt::to_integral(param.enter));
	tls.dispatcher.trigger(afterEnterScene);
}

void ScenesSystem::EnterDefaultScene(const EnterDefaultSceneParam& param) {
	if (param.CheckValid()) {
		LOG_ERROR << "EnterDefaultScene: Invalid parameters";
		return;
	}

	auto defaultScene = NodeSceneSystem::GetNotFullScene({});
	EnterScene({ defaultScene, param.enter });
}

void ScenesSystem::LeaveScene(const LeaveSceneParam& param) {
	if (param.CheckValid()) {
		LOG_ERROR << "LeaveScene: Invalid parameters";
		return;
	}

	auto sceneEntity = tls.registry.get<SceneEntity>(param.leaver).sceneEntity;
	if (!tls.scene_registry.valid(sceneEntity)) {
		LOG_ERROR << "LeaveScene: SceneEntity not valid";
		return;
	}

	auto& scenePlayers = tls.scene_registry.get<ScenePlayers>(sceneEntity);
	scenePlayers.erase(param.leaver);
	tls.registry.remove<SceneEntity>(param.leaver);

	auto* gsPlayerInfo = tls.scene_registry.try_get<GameNodePlayerInfoPtr>(sceneEntity);
	if (gsPlayerInfo) {
		(*gsPlayerInfo)->set_player_size((*gsPlayerInfo)->player_size() - 1);
	}

	AfterLeaveScene afterLeaveScene;
	afterLeaveScene.set_entity(entt::to_integral(param.leaver));
	tls.dispatcher.trigger(afterLeaveScene);
}

void ScenesSystem::CompelPlayerChangeScene(const CompelChangeSceneParam& param) {
	auto& destNodeScene = tls.game_node_registry.get<ServerComp>(param.destNode);
	auto sceneEntity = destNodeScene.GetMinPlayerSizeSceneByConfigId(param.sceneConfId);

	if (sceneEntity == entt::null) {
		CreateGameNodeSceneParam p{ .node = param.destNode };
		p.sceneInfo.set_scene_confid(param.sceneConfId);
		sceneEntity = CreateScene2GameNode(p);
	}

	LeaveScene({ param.player_ });
	if (sceneEntity == entt::null) {
		EnterDefaultScene({ param.player_ });
		return;
	}

	EnterScene({ sceneEntity, param.player_ });
}

void ScenesSystem::ReplaceCrashServer(entt::entity crashNode, entt::entity destNode) {
	auto& crashNodeScene = tls.game_node_registry.get<ServerComp>(crashNode);
	auto sceneLists = crashNodeScene.GetSceneList();

	for (auto& confIdSceneList : sceneLists | std::

		views::values) {
		for (auto scene : confIdSceneList) {
			auto* pSceneInfo = tls.scene_registry.try_get<SceneInfo>(scene);
			if (!pSceneInfo) {
				continue;
			}
			CreateGameNodeSceneParam p{ .node = destNode };
			p.sceneInfo.set_scene_confid(pSceneInfo->scene_confid());
			CreateScene2GameNode(p);
		}
	}

	Destroy(tls.game_node_registry, crashNode);
}
