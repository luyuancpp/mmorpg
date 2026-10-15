#include "aoi.h"
#include "actor/actor_net_id.h"

#include "hexagons_grid.h"
#include "spatial/comp/grid_comp.h"
#include "spatial/comp/scene_node_scene_comp.h"
#include "spatial/constants/aoi_priority.h"
#include "spatial/system/grid.h"
#include "spatial/system/interest.h"
#include "spatial/system/view.h"
#include "macros/return_define.h"
#include "muduo/base/Logging.h"
#include "proto/common/component/actor_comp.pb.h"
#include "proto/common/component/team_comp.pb.h"
#include "modules/scene/scene_events.h"
#include "rpc/service_metadata/player_scene_service_metadata.h"
#include "core/utils/stat/stat.h"
#include "proto/scene/player_scene.pb.h"
#include "network/player_message_utils.h"
#include <modules/scene/comp/scene_comp.h>

// Determine the interest priority tag that |observer| should assign to |target|.
// The tag is a semantic category; the active AoiPriorityPolicy turns it into a
// numeric weight for eviction decisions.
static AoiPriority DetermineAoiPriority(entt::entity observer, entt::entity target)
{
    // Same team -> kTeammate
    const auto* observerTeam = tlsEcs.actorRegistry.try_get<TeamId>(observer);
    const auto* targetTeam   = tlsEcs.actorRegistry.try_get<TeamId>(target);
    if (observerTeam && targetTeam &&
        observerTeam->team_id() != 0 &&
        observerTeam->team_id() == targetTeam->team_id())
    {
        return AoiPriority::kTeammate;
    }

    return AoiPriority::kNormal;
}


void AoiSystem::Update(double delta) {
    for (auto&& [entity, transform, sceneComp] : tlsEcs.actorRegistry.view<Transform, SceneEntityComp>().each()) {
        // Skip invalid scenes
        if (!tlsEcs.sceneRegistry.valid(sceneComp.sceneEntity)) {
            LOG_ERROR << "Scene not found for entity " << entt::to_integral(entity);
            continue;
        }

        // Get current hex grid position
        const auto currentHex = GridSystem::CalculateHexPosition(transform);
        const auto currentGridId = GridSystem::GetGridId(currentHex);

        GridSet gridsToEnter, gridsToLeave;
        auto* gridList = UpdateGridState(entity, sceneComp.sceneEntity, currentHex,
                                        currentGridId, gridsToEnter, gridsToLeave);
        if (gridList == nullptr) continue;

        // Handle entity enter/leave visibility
        HandleEntityVisibility(entity, *gridList, gridsToEnter, gridsToLeave);
    }
}

SceneGridListComp* AoiSystem::UpdateGridState(const entt::entity entity, const SceneEntity sceneEntity, const Hex& currentHex,
                                const GridId currentGridId, GridSet& gridsToEnter, GridSet& gridsToLeave) {
    const auto* previousHexPtr = tlsEcs.actorRegistry.try_get<Hex>(entity);
    if (previousHexPtr != nullptr && hex_distance(*previousHexPtr, currentHex) == 0) {
        return nullptr;
    }

    // 此处延迟创建:真实入格写入口是 SceneGridListComp 的唯一创建点。
    // 空场景和只读空间查询不需要表;首次入格与跨格移动都在这里完成占位。
    auto& gridList = tlsEcs.sceneRegistry.get_or_emplace<SceneGridListComp>(sceneEntity);
    if (previousHexPtr == nullptr) {
        // First time entering scene
        // AoiListComp 不在这里预挂;实际可见关系由 InterestSystem::AddAoiEntity 按需创建。
        gridList[currentGridId].entities.insert(entity);
        tlsEcs.actorRegistry.emplace<Hex>(entity, currentHex);
        GridSystem::GetCurrentAndNeighborGridIds(currentHex, gridsToEnter);
    } else {
        // Position update
        const auto previousHex = *previousHexPtr;

        GridSystem::GetCurrentAndNeighborGridIds(previousHex, gridsToLeave);
        GridSystem::GetCurrentAndNeighborGridIds(currentHex, gridsToEnter);

        // Remove common grids (no need to process enter/leave for unchanged grids)
        for (auto it = gridsToLeave.begin(); it != gridsToLeave.end(); ) {
            if (gridsToEnter.erase(*it)) {
                it = gridsToLeave.erase(it);
            } else {
                ++it;
            }
        }

        const auto previousGridId = GridSystem::GetGridId(previousHex);
        if (auto previousGrid = gridList.find(previousGridId); previousGrid != gridList.end()) {
            previousGrid->second.entities.erase(entity);
        }
        gridList[currentGridId].entities.insert(entity);

        // Hex has const members -> copy assignment is deleted -> emplace_or_replace won't compile.
        // Use remove + emplace instead.
        tlsEcs.actorRegistry.remove<Hex>(entity);
        tlsEcs.actorRegistry.emplace<Hex>(entity, currentHex);
    }
    return &gridList;
}

void AoiSystem::HandleEntityVisibility(entt::entity entity, SceneGridListComp& gridList, 
                                       const GridSet& gridsToEnter, const GridSet& gridsToLeave) {
    EntityUnorderedSet entitiesEnteringView, entitiesLeavingView;

    // Entities in grids the observer is entering
    for (const auto& gridId : gridsToEnter) {
        auto gridIt = gridList.find(gridId);
        if (gridIt == gridList.end()) continue;

        for (const auto& otherEntity : gridIt->second.entities) {
            if (otherEntity == entity) continue;

            if (ViewSystem::CanSee(entity, otherEntity)) {
                const auto priority = DetermineAoiPriority(entity, otherEntity);
                if (InterestSystem::AddAoiEntity(entity, otherEntity, priority)) {
                    entitiesEnteringView.insert(otherEntity);
                }
            }

            if (ViewSystem::CanSee(otherEntity, entity)) {
                const auto priority = DetermineAoiPriority(otherEntity, entity);
                const auto* otherAoi = tlsEcs.actorRegistry.try_get<AoiListComp>(otherEntity);
                const bool alreadyVisible = otherAoi && otherAoi->Contains(entity);
                if (InterestSystem::AddAoiEntity(otherEntity, entity, priority) && !alreadyVisible) {
                    // 静止观察者可能一直不跨格，必须在反向兴趣关系建立时立即通知。
                    NotifyEntityVisibilityChanges(otherEntity, EntityUnorderedSet{entity}, {});
                }
            }
        }
    }

    // Entities in grids the observer is leaving.
    // The observer's own AoiListComp does not change identity during this loop
    // (RemoveAoiEntity only erases entries), so look it up once.
    auto* entityAoi = tlsEcs.actorRegistry.try_get<AoiListComp>(entity);
    for (const auto& gridId : gridsToLeave) {
        auto gridIt = gridList.find(gridId);
        if (gridIt == gridList.end()) continue;

        for (const auto& otherEntity : gridIt->second.entities) {
            // Drop the observer's view of otherEntity, unless that entry is
            // pinned (pinned entries are managed by buff/skill lifecycle).
            if (entityAoi) {
                auto it = entityAoi->entries.find(otherEntity);
                if (it != entityAoi->entries.end() && it->second.priority != AoiPriority::kPinned) {
                    entitiesLeavingView.insert(otherEntity);
                    InterestSystem::RemoveAoiEntity(entity, otherEntity);
                }
            }

            // Reverse direction: remove entity from other's list (unless pinned there).
            auto* otherAoi = tlsEcs.actorRegistry.try_get<AoiListComp>(otherEntity);
            if (otherAoi) {
                auto it = otherAoi->entries.find(entity);
                if (it != otherAoi->entries.end() && it->second.priority != AoiPriority::kPinned) {
                    InterestSystem::RemoveAoiEntity(otherEntity, entity);
                    NotifyEntityVisibilityChanges(otherEntity, {}, EntityUnorderedSet{entity});
                }
            }
        }
    }

    NotifyEntityVisibilityChanges(entity, entitiesEnteringView, entitiesLeavingView);
}

void AoiSystem::NotifyEntityVisibilityChanges(entt::entity entity, 
                                              const EntityUnorderedSet& enteringEntities, 
                                              const EntityUnorderedSet& leavingEntities) {
    // 下面几处 S2C 消息都是"本线程复用的发送缓冲",不是任何实体的数据,所以不挂在 GlobalEntity 上:
    // 那样每次通知都要过一遍 get_or_emplace,而且这块缓冲会以组件身份混进 globalRegistry。
    // 用函数内 static thread_local:与 tlsEcs 同线程、同寿命,Clear() 保留已分配的子消息供下次复用;
    // 填好后同步发送,发送路径不会回调进 AOI,不存在重入覆盖。
    // Notify entities entering view
    if (!enteringEntities.empty()) {
        static thread_local ActorListCreateS2C createMsg;
        createMsg.Clear();
        for (auto& otherEntity : enteringEntities) {
            if (actor_net_id::Of(tlsEcs.actorRegistry, otherEntity) == 0) {
                LOG_ERROR << "AOI: actor create skipped, network id missing";
                continue;
            }
            ViewSystem::FillActorCreateMessageInfo(entity, otherEntity, *createMsg.add_actor_list());
        }
        if (!createMsg.actor_list().empty()) {
            SendMessageToClientViaGate(SceneSceneClientPlayerNotifyActorListCreateMessageId, createMsg, entity);
        }
    }

    // Notify entities leaving view
    if (!leavingEntities.empty()) {
        static thread_local ActorListDestroyS2C destroyMsg;
        destroyMsg.Clear();
        for (auto& otherEntity : leavingEntities) {
            const auto netId = actor_net_id::Of(tlsEcs.actorRegistry, otherEntity);
            if (netId == 0) {
                LOG_ERROR << "AOI: actor removal skipped, network id missing";
                continue;
            }
            destroyMsg.add_entity(netId);
        }
        if (!destroyMsg.entity().empty()) {
            SendMessageToClientViaGate(SceneSceneClientPlayerNotifyActorListDestroyMessageId, destroyMsg, entity);
        }
    }
}

void AoiSystem::BeforeLeaveSceneHandler(const BeforeLeaveScene& message) {
    const auto entity = message.actor;
    if (tlsEcs.actorRegistry.valid(entity)) {
        // 离场者本人的表也必须清空,否则旧场景的条目会占满新场景的观察容量。
        if (auto* ownList = tlsEcs.actorRegistry.try_get<AoiListComp>(entity)) {
            ownList->entries.clear();
        }
        const auto* hex = tlsEcs.actorRegistry.try_get<Hex>(entity);
        const auto* scene = tlsEcs.actorRegistry.try_get<SceneEntityComp>(entity);
        if (hex != nullptr && scene != nullptr && tlsEcs.sceneRegistry.valid(scene->sceneEntity)) {
            // 格子表是按需状态;缺表时仍须继续做远端关系清扫,不得补建空表。
            if (auto* gridList = tlsEcs.sceneRegistry.try_get<SceneGridListComp>(scene->sceneEntity)) {
                GridSet gridsToLeave;
                GridSystem::GetCurrentAndNeighborGridIds(*hex, gridsToLeave);
                RemoveEntityFromGrid(*hex, *gridList, entity);
                BroadcastEntityLeave(*gridList, entity, gridsToLeave);
            }
        }
    }

    // 跨实体关系不能靠 on_destroy:组件销毁顺序不保证 Hex / 场景绑定仍在。
    // 邻格广播之后再扫全部 AOI 参与者,回收远端 Pin 和缺格子记录的残留。
    // 成本为 O(AOI 参与者)/次离场;离场频率低,正确性不依赖空间索引完整。
    EntityUnorderedSet remoteWatchers;
    for (auto&& [watcher, list] : tlsEcs.actorRegistry.view<AoiListComp>().each()) {
        if (watcher != entity && list.entries.erase(entity) > 0) {
            remoteWatchers.insert(watcher);
        }
    }
    for (const auto watcher : remoteWatchers) {
        NotifyEntityVisibilityChanges(watcher, {}, EntityUnorderedSet{entity});
    }
}

bool AoiSystem::PinAoiEntity(entt::entity watcher, entt::entity target) {
    return InterestSystem::PinAoiEntity(watcher, target);
}

void AoiSystem::UnpinAoiEntity(entt::entity watcher, entt::entity target) {
    if (!tlsEcs.actorRegistry.valid(watcher)) return;
    auto* list = tlsEcs.actorRegistry.try_get<AoiListComp>(watcher);
    if (list == nullptr) return;
    const auto entry = list->entries.find(target);
    if (entry == list->entries.end() || entry->second.priority != AoiPriority::kPinned) return;

    bool visible = false;
    if (tlsEcs.actorRegistry.valid(target)) {
        const auto* watcherScene = tlsEcs.actorRegistry.try_get<SceneEntityComp>(watcher);
        const auto* targetScene = tlsEcs.actorRegistry.try_get<SceneEntityComp>(target);
        const auto* watcherHex = tlsEcs.actorRegistry.try_get<Hex>(watcher);
        const auto* targetHex = tlsEcs.actorRegistry.try_get<Hex>(target);
        if (watcherScene != nullptr && targetScene != nullptr && watcherHex != nullptr && targetHex != nullptr &&
            watcherScene->sceneEntity == targetScene->sceneEntity &&
            tlsEcs.sceneRegistry.valid(watcherScene->sceneEntity)) {
            // 先解除强制可见权限再判 CanSee,否则隐身目标会借当前 kPinned 绕过检查。
            entry->second.priority = DetermineAoiPriority(watcher, target);
            GridSet visibleGrids;
            GridSystem::GetCurrentAndNeighborGridIds(*watcherHex, visibleGrids);
            visible = visibleGrids.contains(GridSystem::GetGridId(*targetHex)) && ViewSystem::CanSee(watcher, target);
        }
    }
    if (visible) {
        return;
    }
    InterestSystem::RemoveAoiEntity(watcher, target);
    NotifyEntityVisibilityChanges(watcher, {}, EntityUnorderedSet{target});
}

void AoiSystem::ResetEntityVisibility(entt::entity entity) {
    if (!tlsEcs.actorRegistry.valid(entity)) {
        return;
    }

    // 1) 自己这一侧:摘掉所有非钉住条目,并让自己的客户端销毁这些 actor。
    //    不通知的话客户端会把旧位置的 actor 永远留在世界里(之后同一实体再进视野时
    //    客户端按 entity 去重,连位置都不会更新)。kPinned 由 buff/skill 生命周期管理,不动。
    if (auto* ownList = tlsEcs.actorRegistry.try_get<AoiListComp>(entity)) {
        EntityUnorderedSet leavingEntities;
        for (auto it = ownList->entries.begin(); it != ownList->entries.end(); ) {
            if (it->second.priority == AoiPriority::kPinned) {
                ++it;
                continue;
            }
            leavingEntities.insert(it->first);
            it = ownList->entries.erase(it);
        }
        NotifyEntityVisibilityChanges(entity, {}, leavingEntities);
    }

    // 2) 观察者一侧:按格子建立的条目只存在于"当前格及邻格"的实体之间,遍历旧 Hex 的这 7 格就够了。
    //    Unpin 已立即剔除超出范围的条目,远端剩余条目仍为 kPinned,按约定保留。
    const auto* hex = tlsEcs.actorRegistry.try_get<Hex>(entity);
    if (hex == nullptr) {
        return;
    }

    const auto* sceneEntityComp = tlsEcs.actorRegistry.try_get<SceneEntityComp>(entity);
    if (sceneEntityComp != nullptr && tlsEcs.sceneRegistry.valid(sceneEntityComp->sceneEntity)) {
        if (auto* gridList = tlsEcs.sceneRegistry.try_get<SceneGridListComp>(sceneEntityComp->sceneEntity)) {
            GridSet gridsToLeave;
            GridSystem::GetCurrentAndNeighborGridIds(*hex, gridsToLeave);

            // 先收集再改:RemoveEntityFromGrid 在测试开关下会删空格子,不能边遍历边删。
            EntityUnorderedSet observers;
            for (const auto& gridId : gridsToLeave) {
                auto gridIt = gridList->find(gridId);
                if (gridIt == gridList->end()) continue;

                for (const auto& otherEntity : gridIt->second.entities) {
                    if (otherEntity == entity) continue;

                    const auto* otherAoi = tlsEcs.actorRegistry.try_get<AoiListComp>(otherEntity);
                    if (otherAoi == nullptr) continue;

                    auto it = otherAoi->entries.find(entity);
                    if (it != otherAoi->entries.end() && it->second.priority != AoiPriority::kPinned) {
                        observers.insert(otherEntity);
                    }
                }
            }

            for (const auto& observer : observers) {
                InterestSystem::RemoveAoiEntity(observer, entity);
                NotifyEntityVisibilityChanges(observer, {}, EntityUnorderedSet{entity});
            }

            RemoveEntityFromGrid(*hex, *gridList, entity);
        }
    }

    // 摘掉 Hex:下一次 Update 把它当作首次进场,在新位置重建双向可见性。
    // hex 指针在这一行之后失效,必须放在最后。
    tlsEcs.actorRegistry.remove<Hex>(entity);
}

void AoiSystem::RemoveEntityFromGrid(const Hex& hex, SceneGridListComp& gridList, entt::entity entity) {
    const auto gridId = GridSystem::GetGridId(hex);
    auto gridIt = gridList.find(gridId);
    if (gridIt == gridList.end()) return;

    auto& grid = gridIt->second;
    grid.entities.erase(entity);

    if (gFeatureSwitches[kTestClearEmptyTiles])
    {
        if (grid.entities.empty()) {
            gridList.erase(gridIt);
        }
    }
}

void AoiSystem::BroadcastEntityLeave(const SceneGridListComp& gridList, entt::entity entity, const GridSet& gridsToLeave) {
    if (gridsToLeave.empty()) return;

    const auto netId = actor_net_id::Of(tlsEcs.actorRegistry, entity);

    // 本线程复用的发送缓冲,理由同 NotifyEntityVisibilityChanges。
    static thread_local ActorDestroyS2C destroyMsg;
    destroyMsg.Clear();
    destroyMsg.set_entity(netId);

    EntityUnorderedSet observersToNotify;
    for (const auto& gridId : gridsToLeave) {
        auto gridIt = gridList.find(gridId);
        if (gridIt == gridList.end()) continue;

        for (const auto& observer : gridIt->second.entities) {
            observersToNotify.insert(observer);
            InterestSystem::RemoveAoiEntity(observer, entity);
        }
    }

    if (netId != 0) {
        BroadcastMessageToPlayers(SceneSceneClientPlayerNotifyActorDestroyMessageId, destroyMsg, observersToNotify);
    } else {
        LOG_ERROR << "AOI: actor leave notification skipped, network id missing";
    }
}

