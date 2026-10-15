#include "scene_local_events.h"
#include "thread_context/ecs_context.h"
#include "modules/mission/mission_events.h"
#include "modules/mission/comp/mission_comp.h"
#include "modules/mission/comp/missions_config_comp.h"
#include "modules/mission/system/mission.h"
#include "modules/scene/scene_events.h"
#include "services/scene/player/player_events.h"
#include "services/scene/actor/actor_events.h"
#include "player/system/player_lifecycle.h"
#include "player/system/player_attribute.h"
#include "player/system/player_pet.h"
#include "player/system/player_mission.h"
#include "proto/common/component/player_login_comp.pb.h"
#include "modules/condition/condition_type.h"
#include "combat_state/system/combat_state.h"
#include "infra/agones/agones_gameserver_lifecycle.h"
#include "core/time/system/time.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "muduo/base/Logging.h"

namespace
{
void AcceptMissionEventHandler(const AcceptMissionEvent& event)
{
    const auto entity = event.player;
    // 自动接续也经过正式接取门禁，不能绕过活动时间和真实进度来源限制。
    const auto result = PlayerMissionSystem::Accept(entity, MissionListComp::kPlayerMission,
        event.missionId, TimeSystem::NowMillisecondsUTC());
    if (result != kSuccess) LOG_WARN << "任务链接取被拒 mission=" << event.missionId << " tip=" << result;
}

void ConditionEventHandler(const ConditionEvent& event)
{
    const auto entity = event.player;
    if (!tlsEcs.actorRegistry.valid(entity) || PlayerLifecycleSystem::IsCrossZoneFrozen(entity)) return;
    auto* container = tlsEcs.actorRegistry.try_get<MissionsContainerComp>(entity);
    if (container == nullptr) return;
    auto* comp = container->GetMutable(MissionListComp::kPlayerMission);
    if (comp == nullptr) return;
    MissionSystem::HandleConditionEvent(event, *comp, MissionConfig::GetSingleton());
}

void OnMissionAwardEventHandler(const OnMissionAwardEvent& event)
{
    const auto result = PlayerMissionSystem::ClaimReward(event.player,
        MissionListComp::kPlayerMission, event.missionId);
    if (result != kSuccess)
        LOG_WARN << "任务自动奖励保留待重试 mission=" << event.missionId << " tip=" << result;
}

void RegisterPlayerEventHandler(const RegisterPlayerEvent& event)
{
    auto player = event.player;

	if (!tlsEcs.actorRegistry.valid(player))
	{
		LOG_ERROR << "玩家实体不存在 entity=" << entt::to_integral(event.player);
		return;
	}

	// 必需的默认技能由 InitPlayerFromAllData 显式初始化;本事件只作通知。
}

void PlayerUpgradeEventHandler(const PlayerUpgradeEvent& event)
{
    auto player = event.player;
	if (!tlsEcs.actorRegistry.valid(player))
	{
		LOG_ERROR << "玩家实体不存在 entity=" << entt::to_integral(event.player);
		return;
	}
	// 升级发点:总点数按等级换算,不存量;重算二级属性并推面板(客户端整体覆盖)
	PlayerAttributeSystem::Recalculate(player, PlayerAttributeSystem::RecalcReason::kLevelChanged);
	PlayerAttributeSystem::PushPanel(player);
	// 宝宝等级跟随主人(经验系统未接前这是宝宝唯一的成长通道),重算后推列表
	PetSystem::RecalculateAll(player, PetSystem::RecalcReason::kLevelChanged);
	PetSystem::PushList(player);
    ConditionEvent progress{player, static_cast<uint32_t>(eConditionType::kConditionLevelUp), {event.newLevel}, event.newLevel};
    tlsEcs.dispatcher.trigger(progress);
}

void InitializePlayerCompsEventHandler(const InitializePlayerCompsEvent& event)
{
    auto player = event.player;

	if (!tlsEcs.actorRegistry.valid(player))
	{
		LOG_ERROR << "玩家实体不存在 entity=" << entt::to_integral(event.player);
		return;
	}

    // 任务 scope 已由 mission_marshal::Unmarshal 保证;本事件不重复初始化。
}

void PlayerLoginEventHandler(const PlayerLoginEvent& event)
{
    auto player = event.player;

	if (!tlsEcs.actorRegistry.valid(player))
	{
		LOG_ERROR << "玩家实体不存在 entity=" << entt::to_integral(event.player);
		return;
	}

	const auto enterType = static_cast<EnterGsType>(event.enterGsType);
	LOG_INFO << "玩家登录事件 entity=" << entt::to_integral(event.player)
			 << " enter_gs_type=" << EnterGsType_Name(enterType);

	switch (enterType)
	{
	case LOGIN_FIRST:
		// 待接:全量同步、每日重置检查与欢迎消息。
		break;
	case LOGIN_RECONNECT:
		// 待接:增量同步与恢复中断动作。
		break;
	case LOGIN_REPLACE:
		// 待接:清理旧交易/匹配状态并向新客户端同步。
		break;
	default:
		LOG_WARN << "玩家登录事件类型异常 enter_gs_type=" << event.enterGsType;
		break;
	}
}

void OnSceneCreatedHandler(const OnSceneCreated& event)
{
    // 场景入口仅在实体真实创建/销毁时通知,此处是 Agones 唯一计数点。
    agones::GameServerLifecycle::Instance().OnUnitCreated(entt::to_integral(event.scene));
}

void OnSceneDestroyedHandler(const OnSceneDestroyed& event)
{
    // 场景入口仅在实体真实创建/销毁时通知,此处是 Agones 唯一计数点。
    agones::GameServerLifecycle::Instance().OnUnitDestroyed(entt::to_integral(event.scene));
}

void CombatStateAddedEventHandler(const CombatStateAddedEvent& event)
{
    CombatStateSystem::AddCombatState(event);
}

void CombatStateRemovedEventHandler(const CombatStateRemovedEvent& event)
{
    CombatStateSystem::RemoveCombatState(event);
}

} // namespace

namespace scene_local_events
{
void RegisterMission()
{
    tlsEcs.dispatcher.sink<AcceptMissionEvent>().connect<&AcceptMissionEventHandler>();
    tlsEcs.dispatcher.sink<ConditionEvent>().connect<&ConditionEventHandler>();
    tlsEcs.dispatcher.sink<OnMissionAwardEvent>().connect<&OnMissionAwardEventHandler>();
}

void UnregisterMission()
{
    tlsEcs.dispatcher.sink<AcceptMissionEvent>().disconnect<&AcceptMissionEventHandler>();
    tlsEcs.dispatcher.sink<ConditionEvent>().disconnect<&ConditionEventHandler>();
    tlsEcs.dispatcher.sink<OnMissionAwardEvent>().disconnect<&OnMissionAwardEventHandler>();
}

void RegisterAll()
{
    RegisterMission();
    tlsEcs.dispatcher.sink<RegisterPlayerEvent>().connect<&RegisterPlayerEventHandler>();
    tlsEcs.dispatcher.sink<PlayerUpgradeEvent>().connect<&PlayerUpgradeEventHandler>();
    tlsEcs.dispatcher.sink<InitializePlayerCompsEvent>().connect<&InitializePlayerCompsEventHandler>();
    tlsEcs.dispatcher.sink<PlayerLoginEvent>().connect<&PlayerLoginEventHandler>();
    tlsEcs.dispatcher.sink<OnSceneCreated>().connect<&OnSceneCreatedHandler>();
    tlsEcs.dispatcher.sink<OnSceneDestroyed>().connect<&OnSceneDestroyedHandler>();
    tlsEcs.dispatcher.sink<CombatStateAddedEvent>().connect<&CombatStateAddedEventHandler>();
    tlsEcs.dispatcher.sink<CombatStateRemovedEvent>().connect<&CombatStateRemovedEventHandler>();
}

void UnregisterAll()
{
    UnregisterMission();
    tlsEcs.dispatcher.sink<RegisterPlayerEvent>().disconnect<&RegisterPlayerEventHandler>();
    tlsEcs.dispatcher.sink<PlayerUpgradeEvent>().disconnect<&PlayerUpgradeEventHandler>();
    tlsEcs.dispatcher.sink<InitializePlayerCompsEvent>().disconnect<&InitializePlayerCompsEventHandler>();
    tlsEcs.dispatcher.sink<PlayerLoginEvent>().disconnect<&PlayerLoginEventHandler>();
    tlsEcs.dispatcher.sink<OnSceneCreated>().disconnect<&OnSceneCreatedHandler>();
    tlsEcs.dispatcher.sink<OnSceneDestroyed>().disconnect<&OnSceneDestroyedHandler>();
    tlsEcs.dispatcher.sink<CombatStateAddedEvent>().disconnect<&CombatStateAddedEventHandler>();
    tlsEcs.dispatcher.sink<CombatStateRemovedEvent>().disconnect<&CombatStateRemovedEventHandler>();
}

} // namespace scene_local_events
