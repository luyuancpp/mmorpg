#pragma once

// 收件箱 SCENE_GM_SANCTION 域的执行器(docs/design/player-inbox-service.md §2.2、§7.6)。
//
// 三种类型 → 三个现成原语:
//   GM_BLOCK_CURRENCY_GAIN    → CurrencySystem::BlockCurrency    (天然幂等)
//   GM_UNBLOCK_CURRENCY_GAIN  → CurrencySystem::UnblockCurrency  (天然幂等)
//   GM_ATTACH_CURRENCY_DEBT   → CurrencySystem::AttachDebt       (**不幂等**,只靠收件箱账本去重)
//
// 结局口径:
//   * 载荷解码失败、币种越界、欠款额不在 (0, INT64_MAX] → kRejected(kPlayerInboxPayloadInvalid);
//   * 原语返回 kSuccess → kApplied;原语返回其他 tip 码 → kRejected(该码),例如 AttachDebt 的 owed 溢出;
//   * CurrencyComp / PlayerCurrencyComp 缺失(加载不变量被破坏)→ kDefer:不改任何状态,不把一条 GM 处置
//     因本地缺陷钉成终态 REJECTED;此时存盘本身也会被 Marshal 整份拒绝,结局落不了盘。
//
// 不判冻结 / 战斗 / 可改:那是 PlayerInboxSystem 前置闸的事(闸**不下沉**到 CurrencySystem,D48 红线)。
// 契约见 player_inbox_registry.h 的 PlayerInboxExecutorFn。单测:cpp/tests/currency_test/gm_sanction_inbox_executor_test.cpp。

#include <string>

#include "entt/src/entt/entity/entity.hpp"
#include "player/system/player_inbox_registry.h"

class GmSanctionInboxExecutor
{
public:
	static PlayerInboxExecResult BlockCurrencyGain(entt::entity player, const std::string &payload);
	static PlayerInboxExecResult UnblockCurrencyGain(entt::entity player, const std::string &payload);
	static PlayerInboxExecResult AttachCurrencyDebt(entt::entity player, const std::string &payload);
};
