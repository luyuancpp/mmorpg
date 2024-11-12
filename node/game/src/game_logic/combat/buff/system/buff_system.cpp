#include "buff_system.h"
#include <ranges>
#include "buff_config.h"
#include "game_logic/combat/buff/util/buff_impl_util.h"
#include "game_logic/combat/buff/util/buff_util.h"
#include "proto/logic/component/buff_comp.pb.h"
#include "thread_local/storage.h"

bool CanApplyMoreTicks(const BuffPeriodicBuffPbComponent& periodicBuff, const BuffTable* buffTable) {
    return (buffTable->intervalcount() <= 0) || (periodicBuff.ticks_done() + 1 <= buffTable->intervalcount());
}

void UpdatePeriodicBuff(const entt::entity target, const uint64_t buffId, BuffComp& buffComp, double delta) {
    auto [buffTable, result] = GetBuffTable(buffComp.buffPb.buff_table_id());
    if (!buffTable || buffTable->interval() <= 0) {
        return;
    }

    auto& periodicBuff = *buffComp.buffPb.mutable_periodic();
    double periodicTimer = periodicBuff.periodic_timer() + delta;

    for (uint32_t i = 0; i < 5 && CanApplyMoreTicks(periodicBuff, buffTable); ++i ) {
        if (periodicTimer < buffTable->interval()) {
            break;  // 如果定时器小于间隔，退出循环
        }
        
        periodicTimer -= buffTable->interval();
        periodicBuff.set_ticks_done(periodicBuff.ticks_done() + 1);
        BuffUtil::OnIntervalThink(target, buffId);
    }

    periodicBuff.set_periodic_timer(periodicTimer);
}

void ProcessBuffs(const entt::entity target, BuffListComp& buffListComp, const double delta) {
    for (auto& [buffId, buffComp] : buffListComp) {
        UpdatePeriodicBuff(target, buffId, buffComp, delta);
    }
}

void BuffSystem::Update(const double delta) {
    for (auto&& [target, buffListComp] : tls.registry.view<BuffListComp>().each()) {
        ProcessBuffs(target, buffListComp, delta);
        BuffUtil::RemovePendingBuffs(target, buffListComp);
    }
}


