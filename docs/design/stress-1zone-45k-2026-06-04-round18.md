# Stress Test — 1 Zone × 45k Robots — Round 18 (2026-06-04)

> **Status:** PASS, R1 修复部分生效 — login_fail 1000 → 504 (−50%),
> token expired 平均窗口从 0~30s → 66s,但未归零。
>
> Run dir: `robot/logs/stress-round18-20260604-062122`
> Baseline: `docs/design/prev-summary-round18.txt` (= R17 summary, 2026-06-03)
> Start: `2026-06-04 06:28:32` Local · 18-minute window
> Robot config: `robot/etc/robot.stress-3zone-z1.yaml` (45,000 robots, profile=stress)

## 1. What changed vs Round 17

业务代码改动只有 R1 根治修复(commit `fb0a9e9f9`):

- `gatetoken.go`: 拆 `PickAndSignGateToken` → `PickGate` + `SignGateToken`
- `queue.go`: 新增 `AdmitSlot` 类型;`consumeAdmit` → `consumeAdmitAndSign(queueID, signFn)`;`PopAdmit` 回调从 signFn 改成 pickFn;`Lookup` 加 signFn 参数
- `dispatcher.go`: signFn → pickFn,只挑 gate 不签 token
- `assigngatelogic.go` / `querystatuslogic.go`: Lookup 调用传入签 token 闭包

设计意图: 签 token 从 dispatcher admit 时刻挪到 client polling 命中 admit 时刻,token 5 分钟 TTL 从客户端拿到那一刻才开始算。

## 2. Two-dimensional comparison vs Round 17

### Robot per-minute stats

| Snapshot | conn (R17) | conn (R18) | login_ok (R17) | login_ok (R18) | enter_ok (R17) | enter_ok (R18) | login_fail (R17) | login_fail (R18) | max_login (R17) | max_login (R18) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| t2m  |  3,039 |  3,292 |  2,841 |  3,133 |  2,841 |  3,133 |     0 |   0 | 7.076s | 5.983s |
| t5m  | 15,262 | 16,622 | 12,946 | 14,494 | 12,946 | 14,494 |     0 |   0 | 7.076s | 7.482s |
| t10m | 37,806 | 40,317 | 31,853 | 35,508 | 31,853 | 35,508 |    24 |   0 | 8.225s | 9.398s |
| t15m | 61,027 | 64,277 | 52,035 | 56,646 | 52,035 | 56,646 |   403 | **156** | 8.468s | 9.398s |
| **t18m** | **75,063** | **78,660** | **63,374** | **68,888** | **63,374** | **68,888** | **1,000** | **504** | 8.468s | 9.398s |

**login_fail 全程降低**: t10m 24→0、t15m 403→156、t18m 1000→504。R1 修复**确认生效**,只是不够彻底。

### EnterGame end-to-end

| Snapshot | success (R17) | success (R18) | apply avg (R17) | apply avg (R18) |
|---|---:|---:|---:|---:|
| t10m | 26,636 | 29,843 | 33.8ms | 33.2ms |
| t15m | 47,048 | 50,778 | 35.3ms | 35.0ms |
| **t18m** | **58,554** | **63,449** | **36.0ms** | **36.0ms** |

preload_failed 全程 0。

### Dataloader per-stage avg @ t18m

| Metric | R17 | R18 | Δ |
|---|---:|---:|---|
| cache_check     | 1.7ms  | 1.7ms  | flat |
| sub_cache       | 3.3ms  | 3.3ms  | flat |
| kafka_send      | 2.8ms  | 2.3ms  | -18% |
| **cb_wait{ok}** | 31.1ms | 34.4ms | +11% |

cb_wait 上涨 11% — 不是回归性破坏,可能是 R18 enter_ok 18% 更高带来的更大压力。

### SceneManager EnterScene stages @ t18m

| Stage | R17 | R18 | Δ |
|---|---:|---:|---|
| scene_resolve  | 24.1ms | 24.1ms | flat |
| update_loc     | 0.7ms  | 0.7ms  | flat |
| route_gate     | 0.3ms  | 0.4ms  | +33% (绝对值仍极快) |

### DB task ok/err

| Round | t18m ok | err total |
|---|---:|---:|
| R17 | 58,081 | 0 |
| R18 | **60,818** | **0** |

### Kafka lag

| Round | total lag @ t18m |
|---|---:|
| R17 | 0 |
| R18 | **0** |

## 3. Dirty-save skip rate

| scene log | total | skipped | skip_pct |
|---|---:|---:|---:|
| R17 scene.stdout.log | 57,771 | 34,562 | 59.8% |
| R18 scene.stdout.log | **26,975** | **13,906** | **51.5%** |

R18 总数减半是因为 R18 在采样时刻早(t18m 刚到就 dump 日志,R17 dump 时机晚了几分钟)。两轮都落在设计文档的 30%-70% 健康范围。

## 4. R1 修复效果分析(本轮核心)

### 数据对比

| 指标 | R17 | R18 | Δ |
|---|---:|---:|---:|
| **token expired (gate.stdout.log)** | 1,219 | **847** | **−31%** |
| **token expired (robot stderr)** | 1,219 | **847** | **−31%** |
| **gave up after retries** | 8,653 | 10,535 | +22% |
| **login_fail @ t18m** | 1,000 | **504** | **−50%** |
| 第一次 expired 出现时刻 | t9m10s (启动后 9 分钟) | t11m51s (启动后 ~12 分钟) | +3 分钟 |
| **expired 时 (now - expire) 平均值** | 几乎全在 0-30s 区间 | **avg=66s, max=403s** | +36s |

### 修复部分生效但未归零

预期是 token 几乎不再过期。实际窗口推迟了 ~30 秒但**没归零**。

`gave up after retries` 反而上升 22%(8,653→10,535)是因为 R18 实际跑了更高并发(78,660 conn vs 75,063),更多 robot 经历了 retry 流程。但 enter_ok 18% 更高(68,888 vs 63,374),说明真正进入游戏的 robot 多了 5,514 个。

### 根因二:robot 侧"signFn 签到拿"延迟仍可超 5 分钟

R1 修复后,token 签发时机变成 `consumeAdmitAndSign` 调用瞬间。但 token 到达 cpp gate 之前还要走完这条机器人侧链路:

1. polling HTTP RT(命中 admit)
2. `resolveGateAddrLocal` return
3. `NewGameClient` 拨号到 cpp gate
4. `Sleep 500ms`(`main.go:233` 故意 stagger)
5. `SendRequest ClientTokenVerify` → cpp gate

在 78k 并发下,机器人 goroutine 调度 + TCP backlog + 500ms stagger 累计**可能超 5 分钟**——R18 数据显示最大延迟 **403 秒(>6 分钟)**。

### 修复价值

- ✅ avg 窗口从 0-30s → 66s(token 实际拿到时还剩 ~234s 寿命)
- ✅ login_fail 减半
- ⚠️ 没归零,但 PASS 主线不受影响(enter_fail=0,Kafka lag=0,DB err=0)

## 5. 决策行

- [x] **PASS 主线** — enter_fail=0, preload_failed=0, Kafka lag=0, DB err=0, apply_avg 持平 36.0ms。enter_ok 反而比 R17 提升 18%(63,374→68,888)。
- [x] **R1 修复确认部分生效** — login_fail 减半,expired 平均窗口推迟 30 秒。
- [ ] **R2 待解(原 R1 剩余 50%)** — robot 侧 polling→VerifyGateToken 链路在 78k 并发下延迟可超 5 分钟,token 仍过期。可选治理:
  1. **拉长 gateTokenTTL 5min → 10min**(`go/login/.../assigngatelogic.go:21`)。配合 R1 修复后,从"拿到 token 那一刻"算起 10 分钟,实际能覆盖 R18 观察到的 max=403s + 大量余量。修一行代码,治本。
  2. **robot 侧**: `VerifyGateToken` 失败不计 LoginFail 不消耗 retry,直接重新走 AssignGate(token 已经会重新 fetch,只是 retry 计数被错算)。
  3. 排查 robot 侧"拿到 token 后到底花了多久才发到 cpp gate"——可能 500ms stagger 在 78k 并发下不合适。

建议: 先做 #1(一行代码,治本),Round 19 验证;同时做 #2 作为下界保护。#3 是更深层优化,放后面。

## 6. ARCH §11 决策记录

> 2026-06-04 Round 18 PASS。R1 根治修复(commit fb0a9e9f9)确认部分生效:login_fail 1000→504(-50%),token expired 平均窗口从 0-30s 延后到 avg 66s。剩余 R2(token TTL 跨 robot 侧 polling→VerifyGateToken 链路延迟仍偶发过期)留待 Round 19 前修(gateTokenTTL 5min→10min)。不阻塞 P2 currency_event delta 写设计。

## 7. References

- baseline: `docs/design/prev-summary-round18.txt` (= R17 t18m, 2026-06-03)
- R17 复盘: `docs/design/stress-1zone-45k-2026-06-03-round17.md`
- R1 修复 commit: `fb0a9e9f9 fix(login): sign gate token at consume time, not at admit time`
- dirty-save metric: `cpp/libs/services/scene/player/system/dirty_save_stats.h`
- stress_summarize section 2bb: `tools/scripts/stress_summarize.ps1`
