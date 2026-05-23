# 补缴系统(Deferred Clawback)旁路审计报告

> **审计日期**: 2026-05-15
> **审计目标**: 确认 `CurrencySystem::AddCurrency` 是货币写入的**唯一入口**,补缴 hook 不会被绕过
> **审计依据**: `docs/design/single_player_rollback.md` "Deferred Clawback / 补缴系统" 章节,§"Implementation Key Point" 要求「**must hook into the single unified currency-add function**」
> **结论**: ✅ **PASS** — 架构本身就是防旁路设计,无外部代码可绕过 hook

---

## 1. 为什么这件事重要

补缴系统的工作原理(`currency_system.cpp:95-128`):

```cpp
// AddCurrency(player, gold, 1000) 调用时:
//   1. 拿到当前 debt = 800 gold
//   2. 自动扣 deduct = min(1000, 800) = 800 进债务池
//   3. 玩家实际收到 200 gold
//   4. 写 TX_DEFERRED_CLAWBACK 日志
```

这要求**所有给玩家加货币的代码路径**都必须经过 `AddCurrency`。一旦有任何一处旁路(直接 `*balance += x` 或 `mutable_values()->Set(idx, ...)`)绕过这个入口,补缴对该玩家失效,Bug 利用者就能继续刷取。

## 2. 审计方法

### 2.1 grep 全部潜在旁路写入点

```bash
grep -rn "mutable_values\(\)\|values\(\)->Set\|set_values\(" cpp/
```

**结果(只看业务代码,跳过 generated proto / 第三方库)**:

| 文件 | 行 | 调用 | 性质 |
|---|---|---|---|
| `cpp/libs/modules/currency/system/currency_system.cpp` | 24 | `auto *values = currency.mutable_values();` | ✅ 私有,在 `EnsureCurrencySlots` 内,只填 0 占位,不写余额 |
| `cpp/libs/modules/currency/system/currency_system.cpp` | 49 | `auto *values = currency->mutable_values();` | ✅ 私有,在 `ResolveCurrencyField` 内,**返回指针**给 `AddCurrency`/`DeductCurrency` 私有使用 |

**0 处外部业务代码访问 `mutable_values()`**。

### 2.2 grep 直接持有 CurrencyComp 写指针的代码

```bash
grep -rn "CurrencyComp[^a-zA-Z_]\|currency_comp\.\|currency\.values" cpp/
```

**结果**:所有读写都在 `currency_system.cpp` / `currency_system.h` / `player_currency_comp.h`(load/save proto 的序列化辅助)内。**业务代码无直接访问**。

### 2.3 检查 GM / 系统赠送 / 充值 / 任务奖励等路径

| 来源 | 代码路径 | 调用方式 | 是否触发补缴 |
|---|---|---|---|
| 任务奖励 | `quest/*` → `CurrencySystem::AddCurrency` | 走入口 | ✅ 是 |
| GM 命令(GM_GRANT)| `gm_handler` → `CurrencySystem::AddCurrency` | 走入口 | ✅ 是 |
| 系统活动 | `event_system/*` → `CurrencySystem::AddCurrency` | 走入口 | ✅ 是 |
| NPC 商店卖物 | `shop_service` → `CurrencySystem::AddCurrency` | 走入口 | ✅ 是 |
| 邮件附件 | `mail_service` → `CurrencySystem::AddCurrency` | 走入口 | ✅ 是 |
| 充值 | go-zero 充值流程 → Kafka → `CurrencySystem::AddCurrency` | 走入口 | ✅ 是 |
| **回档恢复** | `data_service` → 直接覆盖 player Redis blob | **绕过** | ❌ **故意如此** |

**「回档绕过补缴」是设计意图**:回档时整个 `player_database` blob 被替换为快照内容,这本来就是「时光倒流」级操作,不应再扣债。`PlayerCurrencyComp::LoadFromProto`(`player_currency_comp.h:32`)会同步把快照里的 debt 状态也加载回去,所以一致性不破。

### 2.4 防御性 doc

已在 `currency_system.h` 顶部加 ⚠️ 标注,列明:
- 哪些写法会破坏补缴不变量
- 唯一允许的非补缴路径(`TX_ROLLBACK_RESTORE` / `TX_SYSTEM_GRANT`)及使用条件
- 本次审计的执行命令和命中数(2026-05-15: 2 hits, both internal)

未来工程师改动 currency 相关代码前会先看到这段警告。

## 3. 已知限制

1. **审计只覆盖当前代码库快照**。每次新增 / 改动 currency 写入逻辑(尤其是新模块),都应重跑 §2.1 的 grep,确认没有引入新旁路。**建议加入 PR review checklist**:任何 PR 触及 `cpp/libs/modules/currency/` 都必须证明仍走 `AddCurrency` 入口
2. **C++ 端检查只覆盖游戏内运行时**。go-zero 服务侧若有「直接 SQL UPDATE player_database SET currency_xxx = ...」的写入,会绕过整个 ECS,补缴失效。**当前未做这种 SQL 旁路审计**(grep `go/` 没看到,但需要持续警惕)
3. **`DeductCurrency` 不被补缴拦截**(只有 Add 拦截),这是设计意图 —— 补缴是从「玩家未来的收入」中扣,扣花费没意义。无需审计

## 4. 持续保障建议

| 措施 | 执行频率 | 当前状态 |
|---|---|---|
| §2.1 grep 自查 | 每次 currency 模块 PR | ⚠️ 未自动化,需 reviewer 手动跑 |
| `currency_system.h` 顶部警告 doc | 一次性 | ✅ 已加(2026-05-15) |
| Currency 模块 owner | 长期 | 未指定 |
| 单元测试覆盖补缴 hook | 一次性 | 未确认是否有专门 test |

## 5. Changelog

- **2026-05-15 v1**: 初次审计,grep 命中 2 处,均在 currency_system.cpp 内部。无外部旁路。补缴不变量保持。在 currency_system.h 顶部加防御性警告 doc。
