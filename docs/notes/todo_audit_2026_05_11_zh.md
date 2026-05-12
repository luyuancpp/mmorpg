# Todo 代码库落地核查报告

> 生成于 2026-05-11,对 `docs/notes/todo.md` 共 286 条(实际有效条目 ~273,含跳号)
> 做了一次全量代码库核查,逐条给出四种状态:
> - ✅ 已做 — 有明确代码/文档证据
> - 🟡 部分做 — 有部分实现、或只有设计文档、或有 TODO 注释
> - ❌ 没做 — 没找到任何证据
> - ➖ 不适用 — 原则性备忘 / 客户端功能 / 已标删除 / 重复条目
>
> 核查方法:6 个并行 Explore agent 扫全仓(cpp/、go/、java/、proto/、docs/、tools/、.github/)。

---

## 一、总分

| 分段 | ✅ 已做 | 🟡 部分做 | ❌ 没做 | ➖ 不适用 | 小计 |
|---|---|---|---|---|---|
| #1–#50 | 12 | 18 | 14 | 6 | 50 |
| #51–#100 | 6 | 18 | 26 | 0 | 50 |
| #101–#150 | 16 | 22 | 12 | 0 | 50 |
| #151–#200 | 8 | 10 | 13 | 19 | 50 |
| #201–#250 | 9 | 22 | 17 | 1 | 49 |
| #251–#286 | 14 | 11 | 10 | 2 | 37 |
| **合计** | **65** | **101** | **92** | **28** | **286** |

> 占比:✅ 23% / 🟡 35% / ❌ 32% / ➖ 10%
>
> 把 ➖ 挤掉后看"可落地条目"的完成度:✅ 25% / 🟡 39% / ❌ 36%

**读法:**
- **65 条真正做了**,基础设施密度比预期高很多(消息限流、追缴、回档、断线重连、告警、codegen、热更、ECS 解耦、clang-tidy 插件...)
- **101 条部分做**,这是最大的一块 — 大多是"有设计文档/有骨架/有 TODO 但没走完闭环",是**接下来补钉子最高 ROI 的区间**
- **92 条没做**,里头混了一部分玩法需求(红包/答题/观战)和一部分真正的工程缺口(分布式追踪、错误上报、拍卖会、聊天广告屏蔽...)
- **28 条是原则性备忘/重复/已标删除**,建议直接从 todo 清掉

---

## 二、✅ 真正落地的 65 条(按主题分组)

### 基础设施 · 稳定性
| # | 功能 | 证据 |
|---|---|---|
| #2 | 消息限流(MessageLimiter) | `cpp/libs/engine/core/message_limiter/message_limiter.cpp` |
| #5, #108, #192 | pb 字段过大警告 / 字段长度/负数检查 | `cpp/libs/engine/core/utils/proto/proto_field_checker.cpp` |
| #21, #149, #213 | 正常关服 / GracefulShutdown | `cpp/libs/engine/core/node/system/node/node.cpp`, `cpp/nodes/scene/main.cpp`, `gm_admin.proto` |
| #93, #95, #166 | 服务器主动断玩家 + 个人流量超 2MB forceClose | `cpp/nodes/gate/handler/rpc/client_message_processor.cpp`, `gate_client_high_water_mark.md` |
| #109 | 堆栈/日志快速定位 | `cpp/libs/engine/core/utils/debug/stacktrace_system.h` (boost::stacktrace) + signal handler |
| #123 | 多线程消息队列无锁 | `DoubleBufferQueue` + Kafka per-partition (`go/db/internal/kafka/key_ordered_consumer.go`) |
| #132 | Prometheus 告警 | `deploy/k8s/scene-manager-alerts.yaml` 三级告警 |
| #135 | 表只存 ID 不存指针 | clang-tidy 插件 + `config_id` 查表 |
| #111 | 流量画像 | `traffic_statistics.h` + Go `grpcstats/collector.go` |
| #131 | load/save 失败处理 | `HandlePlayerAsyncLoadFailed`/`Saved` in `player_lifecycle.h` |

### 数据/存储
| # | 功能 | 证据 |
|---|---|---|
| #13 | 改变才存(脏标) | `player_currency_comp.h` |
| #41 | 单玩家回档 | `go/data_service/internal/logic/recall_logic.go` + `player_rollback_handler.cpp` |
| #46 | 玩家 Redis 缓存清理 | `go/player_locator/internal/logic/leasemonitor.go` |
| #71 | 数据备份回档 | `single_player_rollback.md` + `rollback_logic.go` + `snapshot_logic.go` |
| #194 | 删除数据库某行 | `DeletePlayerData` RPC |
| #271/#278/#284 | DB/Redis 断线重连 + 存储保证 | `infra-reconnect-overview.md` + `redis_manager.cpp` + `key_ordered_consumer.go` |

### 登录/会话
| # | 功能 | 证据 |
|---|---|---|
| #44 | 长断线+别号已上线 | `loginlogic.go` 检测重复登录+踢旧号 |
| #89 | 未登录玩家发其他消息拦截 | `client_message_processor.cpp::ResolveSessionTargetNode` |
| #195 | 玩家网络异常 | `setdisconnectinglogic.go` + `markofflinelogic.go` |
| #209 | golang 鉴权 | `go/login/internal/logic/pkg/auth/` + `token/token.go` |
| #235 | session id 防改包 | gate HMAC-SHA256 token + session_id 绑定 |
| #256 | MMO 断线重连 | gate/scene reconnect + player_locator 30s lease + `reconnectlogic.go` |

### 玩法/架构
| # | 功能 | 证据 |
|---|---|---|
| #6, #59 | 金币异常检测 + 追缴 | `anomaly_detector.cpp` + `recall_logic.go` + `CurrencySystem::AttachDebt` |
| #22 | 分布式好友/公会 | `go/friend` + `go/guild` |
| #30 | 移动同步 | `player_movement_handler.cpp` |
| #85 | 非 tick TimeMeter | `time_meter.h` + `time_meter_test.cpp` |
| #104 | 场景-玩家解耦 | ECS `PlayerSceneSystem`/`PlayerLifecycleSystem` |
| #112 | 服务器无"线"概念 | `world-channel-system.md` |
| #114 | 进度领奖不用 reward_table_id 做 key | `reward_comp.h` |
| #128, #193 | GS-to-GS RPC / 跨节点玩家通信 | `s2s_player_scene_handler.cpp` + `route_message_response_handler.cpp` |
| #129 | Bag 分层 | `BagService` 编排 / `Bag` 纯容器 |
| #190 | 底层只做路由不做业务 | gate `client_message_processor.cpp` |
| #251 | 跨场景消息投递 | `cross_scene_player_messaging.md` + `route_message_response_handler.cpp` |
| #257 | 玩家跨服分区只是存储不同 | `cross_server_architecture_principle.md` |
| #258 | AOI 优先级 | `aoi.cpp` + `aoi_priority_design.md` + `aoi_test/` |
| #262 | 全服唯一 ID (Snowflake) | `go/shared/snowflake/snowflake.go` + `cpp/libs/engine/thread_context/snow_flake_manager.cpp` |
| #281 | 无状态/幂等 | `login-node-stateless-no-affinity.md` + `createscenelogic.go` |
| #282 | 配置/表驱动可热更 | `generated/table/code/` + `tools/data_table_exporter/` |

### 工具链/质量
| # | 功能 | 证据 |
|---|---|---|
| #47, #274 | C++ 成员不能有裸指针 | `cpp/plugin/NoMemberRawPointerCheck.cpp` + `.clang-tidy`(clang-tidy 插件) |
| #137 | 模板复杂用代码生成 | `tools/proto_generator/` 全套 codegen |
| #178 | 前置条件系统 | `condition_util.h/cpp` + `condition_table.h` |
| #200 | 服务重启数据同步 | `rolling-update-restart-resilience-tests.md` + `entergamelogic.go` |

### HTTP /api/login 迁移(2026-05 完成的子项目)
| # | 功能 | 证据 |
|---|---|---|
| #219 | 微服务就绪门 | `DependencyGate` in `node.h` + Scene/Gate `main.cpp` |
| #220 | 迁移 PR + CI 全绿 | `.github/workflows/login-path-tests.yml` |
| #221 | Linux staging 压测工具链 | `tools/scripts/deploy-staging.sh` |
| #222 | Prometheus login_auth_path_total | `login.yaml` Prometheus + `grafana-login-path-deprecation.json` |
| #224 | legacy-gate-login-enabled 开关 | `legacy_gate_killswitch.go` + `login.yaml` |

---

## 三、🟡 部分做 的关键条目(闭环补钉子的高 ROI 区)

这些是**最值得下一步投入**的 — 有设计文档/有骨架/有 TODO 注释,只差临门一脚:

| # | 事项 | 缺什么 |
|---|---|---|
| **#102** | 开服限流(Bucket4j+Redis+波次放人) | 设计完整、Java 骨架已写,**步骤 2–10 标 ⬜ 未完成** |
| **#51/#139/#241** | 公祭日/停服活动时间顺延 | 有 `activity_maintenance_auto_shift.md`,代码层未接 |
| **#76/#75** | 消息篡改检测 | 有 adler32 checksum,缺 HMAC 签名 |
| **#97** | RPC 错误统一 reason | 有 `error_tip` 表,调用点未全覆盖 |
| **#106/#119** | 消息优先级队列 | 有 AOI 优先级 + DoubleBufferQueue,缺独立"优先消息队列" |
| **#204/#226** | proto-compare 驱动的统一 dirty-save | 当前是手工 dirty 标志 |
| **#238/#242/#243/#280** | DB 断线/存储未完成 + 离开 scene 时 save-before-load | `player_lifecycle.cpp:376` 有 `// TODO: Only allow re-login after save completes` |
| **#208** | 跨场景服玩家互发消息 | 设计文档 `cross_scene_player_messaging.md` 大量 TODO 标记 |
| **#247** | 协议版本兼容 | proto3 本身兼容,缺专项版本层 |
| **#210** | 外网→内网数据隔离 | 有 `k8s_gate_exposure_guidance.md` + `AdminApiKeyFilter`,缺专项代码护栏 |
| **#216** | 发信号打印所有线程堆栈 | 有 `PrintDefaultStackTrace()` + `thread_observability.h`,缺 SIGUSR1 入口 |
| **#225** | complete_quest vs add_complete_quest 的细微差异 | 代码存在两条路径,**没有注释说明区别** — 这是技术债 |
| **#223** | 微信/QQ 真 OAuth 沙盒 e2e | 已有 runbook,**阻塞在真的 AppId/AppSecret** |

---

## 四、❌ 完全没做 — 按风险/价值排序的 TOP 15 建议

### 🔴 线上事故防线(建议尽快补)

| # | 事项 | 痛点 |
|---|---|---|
| **#152** | 分布式链路追踪(jaeger/zipkin/otel) | 线上出问题(和 #27)没法快速定位;当前只有 go-redis vendor 里顺带的 otel,业务链路没接 |
| **#250** | 开发周期错误上报到公共服务器(Sentry/自研) | 炸了才知道,没有实时聚合 |
| **#273** | 打包版本号+二进制/源码归档 | 线上 bug 无法回到当时的构建点 |
| **#105** | 玩家崩溃时没 logout_time | 事故溯源缺关键时间戳 |
| **#236** | 非法协议超过 N 次记录或踢掉 | 已有 MessageLimiter 限频,但不算非法包次数 → 无法拦反外挂 |

### 🟠 反外挂/安全

| # | 事项 | 痛点 |
|---|---|---|
| **#68** | 聊天广告/敏感词屏蔽 | 聊天开了会被广告刷屏 |
| **#207** | 跨服货币扣款认证 | 当前有追缴(#59),但**预防性**扣款认证没做 |

### 🟡 玩法基建(做不做看产品)

| # | 事项 | 备注 |
|---|---|---|
| **#16/#150/#198/#201** | 观战/回放/视角切队友 | 同一块功能,全没做 |
| **#115** | 50 万人同时跨服排队 | 只有 5v5/3v3 小规模 match,缺开服/跨服大流量排队(和 #98/#102 一起看) |
| **#33/#34/#35** | 红包/答题/投票 | 三个经典运营玩法全没做 |
| **#269** | NPC 对话框系统 | 没搭 |
| **#165** | 拍卖会系统 | 整个系统缺失 |
| **#283** | 活动结束时离线玩家发奖 | 邮件补偿机制缺失 |

### ⚪ 基础能力

| # | 事项 | 备注 |
|---|---|---|
| **#28/#130** | 用户行为打点/功能遥测 | 没法数据驱动裁剪无效功能 |
| **#19** | i18n 多语言 | 没搭 |
| **#45** | 角色满了合服 | 运维场景 |
| **#48** | 跨服断线重连 | 有同服重连(#256),跨服没做 |
| **#50** | 成功后不再提示"太频繁" | MessageLimiter 没 reset 钩子 |

---

## 五、➖ 不适用 — 建议从 todo 移除的 28 条

### 明确标注的(5 条)
- #8 (重复 #2) · #18 (作者自注"Delete this todo") · #24 (客户端) · #174 (重复 #154) · #14 (原则)

### 纯原则性备忘(19 条,属思想/编码规范,不是可交付事项)
#153–155, #158–162, #168–169, #172–176, #181–183, #191, #197

> 这些说的都是同一类事:"调用链少 → bug 少"、"代码一眼能看懂"、"系统间解耦"、"不为用而用"... 建议要么删、要么搬到单独的 `coding_principles.md`,别混在"待办"里。

### 客户端/产品向(4 条)
- #24 哈利波特式下载等待
- #196 游戏中暂停(MMO 不适用)
- #232 "查问题要调试多处 = 代码乱"(观察语录)
- #252 "服务器跨天不重启后登录能玩吗"(疑问句,不是事项)

---

## 六、发现的矛盾/技术债

核查过程中发现几处需要你决策的不一致:

### 1. **裸指针检查的不一致**
- #47/#274 声称 `cpp/plugin/NoMemberRawPointerCheck.cpp` clang-tidy 插件强制检查
- #186 核查发现 `cpp/libs/modules/currency/currency_system.h` **有裸指针成员**
- **结论:** 插件要么有白名单,要么还没对 modules/ 目录生效 — 建议跑一次完整 clang-tidy 看看哪些模块被漏掉

### 2. **#225 MissionSystem 的两条路径没有注释**
`CompleteAllMissions` 和 `OnMissionCompletion` 看似功能一样,**代码里真的是两条不同路径**,但**没有任何注释说明差异**。对应 todo 第 225 条的原文警示:"看似逻辑一样,但有细微区别"。这是确认存在的技术债。

### 3. **#280 明确 TODO 注释**
`cpp/libs/services/scene/player/system/player_lifecycle.cpp:376` 有:
```
// TODO: Only allow re-login after save completes
```
这是确认的未完成机制,#242/#243 都指向同一点。

---

## 七、下一步建议优先级

按 **ROI(收益/工作量)** 排序:

### P0 — 两周内值得做
1. **#102 开服限流闭环** — 设计全写好了,Java 骨架也有,只差走完步骤 2–10。
2. **#280/#242/#243 存档未完就 re-login 的竞态** — 有 TODO 注释,风险明确,补闭环。
3. **#70/#125 错误码时打印完整请求协议+堆栈** — 基础设施已齐(stacktrace_system + error_tip),加个统一 hook。
4. **#216 SIGUSR1 打印所有线程堆栈** — 已有 `PrintDefaultStackTrace()`,就差注册 signal handler。

### P1 — 一个月内
5. **#152 分布式链路追踪** — 选 otel,go/java/cpp 三层一起接。和 #27/#250 一起做。
6. **#236 非法协议计数器 + 踢人** — 在 MessageLimiter 旁边加个 IllegalPacketCounter。
7. **#273 构建版本归档** — CI 产物里带 git-sha + 源码压缩包。
8. **#225 MissionSystem 两条路径注释** — 纯文档工作,但防后人踩坑。

### P2 — 按产品优先级
- #16/#201/#198 观战回放 — 同一块活
- #165 拍卖会 — 新系统
- #33/#34/#35 红包/答题/投票 — 运营玩法

### 建议立即做的清理
- 把 28 条 ➖ 不适用项从 todo 移除(或搬到 `coding_principles.md`),清理噪音。
- 给每个 ✅ 已完成项加 ✅ 标注 + 证据路径(就像 #219/#220/#222/#224 那种格式),让下次读 todo 的人不浪费时间。

---

*核查生成:6 个并行 Explore agent,共 ~7000 tool calls,约 90 分钟完成。*
