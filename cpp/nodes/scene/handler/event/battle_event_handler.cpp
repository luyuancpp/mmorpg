#include "battle_event_handler.h"
#include "thread_context/ecs_context.h"

///<<< BEGIN WRITING YOUR CODE
#include "battle/system/player_battle.h"
///<<< END WRITING YOUR CODE
void BattleEventHandler::BattleSettlementEventHandler(const BattleSettlementEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
    // battle --Kafka SceneCommand{DispatchEvent}--> DispatchSceneKafkaCommand 白名单解包后直接调用此 handler(loop 线程)。
    // scene 是结算的唯一应用者:battle_id 匹配校验、离线 pending 暂存、锁清理与
    // 客户端通知全部在 PlayerBattleSystem::ApplySettlement 内闭环。
    PlayerBattleSystem::ApplySettlement(event);
///<<< END WRITING YOUR CODE
}
void BattleEventHandler::BattleConfirmedEventHandler(const BattleConfirmedEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
    // battle CreateBattle 成功后每玩家一条(同一 Kafka 通道进来,loop 线程)。
    // PREPARING -> FIGHTING、作废期限切正式 deadline、battle:lock 条件续期,幂等处理全在系统内。
    PlayerBattleSystem::ConfirmBattle(event);
///<<< END WRITING YOUR CODE
}
