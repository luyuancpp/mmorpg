# 装备强化与镶嵌(装备属性系统二期)

> 落地日期:2026-10-09 / 10。**2026-10-13 起已在 main**:正式导表与发号、C++ 编译、C++ / Go 单测、起栈冒烟(一期 + 二期两遍)都有运行证据,
> 见 **§11(现状以 §11 为准)**;客户端界面入口仍未做。
> §9 是当时写给 Codex 的清单、§10.8 是当时的验证记录,两处「未编译 / 未生成」的说法是历史原文,没有回头改。
> 前置:[equipment-attributes.md](equipment-attributes.md)(一期:随机属性 + 穿脱 + 战斗类属性)。本文只写二期新增的部分,
> 一期的表、协议、函数沿用不重复;两份文档冲突时,二期范围内以本文为准。
> 服务器落点(开发时在隔离工作树 `E:\work\xuanming-server-mmo-equip` 的 `feat/equip-enhance-inlay`;**该分支停在历史改写前的旧历史上,已作废,不要再合**,代码以 main 为准):
> 配表 `data/EquipEnhance.xlsx` / `data/EquipGem.xlsx` / `data/EquipGemSocket.xlsx` + `data/Item.xlsx` 新行 + `data/tip/Tip.xlsx` 新码;
> 协议 `proto/common/component/item_base_comp.proto`、`proto/scene/player_bag.proto`、`proto/common/rollback/transaction_log.proto`;
> 纯规则 `cpp/libs/services/scene/player/system/equip_attribute_rules.h`;编排 `cpp/libs/services/scene/player/system/player_equip.{h,cpp}`;
> 容器编排 `cpp/libs/modules/bag/bag_service.{h,cpp}`;入口 `cpp/nodes/scene/handler/rpc/player/player_bag_handler.{h,cpp}`;
> 冒烟 `robot/equip_forge_smoke_scenario.go`。
> 客户端落点:**隔离工作树 `E:\work\mmorpg-client-forge`**(分支 `feat/equip-enhance-inlay`)的 `Game/PlayerFeatures`(数据层)与
> `UI/Ugui/Gameplay/EquipBagModel.cs`(纯逻辑模型)。**不要写 `E:\work\mmorpg-client`**:那是共享的 main 工作区,有每小时自动提交
> 和别的会话的在途文件,而这批客户端代码在生成物到位前有预期的编译错误(§6.5)。
>
> 本文 §2–§6 是**实现契约**:字段号、表列、函数签名以此为准;实现与本文不一致时改其中一个并在 §10 记录。
> 契约经过一轮三视角对抗评审(服务器可行性 / 资产一致性 / 协议与客户端),采纳的修订已并入正文。

## 1. 目标与非目标

### 1.1 需求(2026-10-09)

一期落地后,装备只能「掷属性、穿、脱」。用户要求把装备系统的逻辑代码做完:**强化**(装备 +N,放大基础属性)与
**镶嵌**(宝石嵌进装备孔位提供属性,可取下)。

### 1.2 本次做

1. 强化:消耗金币与材料,按成功率把装备强化等级 +1;等级按万分比放大这件装备的**基础属性**(不放大随机属性与宝石)。
2. 镶嵌 / 取下:宝石是一种可叠加物品;嵌入时消耗 1 颗,记在装备实例上;取下时退回 1 颗。
3. 强化等级与已镶宝石随装备实例走:存档、跨区、穿脱都不丢。
4. 强化与宝石的加成进角色面板与回合制战斗(走一期同一条链:`CollectBonus → Recalculate → 战斗快照`)。
5. 规则全部数据化(3 张新表);客户端零配表,显示所需的一切由服务器下发。
6. 客户端数据层(请求 / 回包 / 快照 / 纯逻辑模型)与用例;robot 冒烟;C++ 单测。

### 1.3 本次不做

| 不做 | 说明 |
|---|---|
| 洗练 / 炼化(重掷蓝 / 粉 / 黄) | 仍按一期 §1.3;`equiprules::RollAffixes` 可直接复用 |
| 宝石合成 / 升级、打孔、孔位解锁 | 孔位由 `EquipGemSocket` 表按部位给定;要按装备档次区分时给该表加列 |
| 强化保护符、祝福值、强化转移 | 失败惩罚只有「掉级」一个旋钮(表列,默认 0) |
| 界面(美术 UI) | 客户端 UI 文件由 Codex 在做;本次只交数据层与纯逻辑模型,入口接线见客户端 `Docs/EquipmentForge-20261009.md` |
| 流水带实例数据 | 一期 §8 已记:流水只有 guid / config / size;带强化 / 宝石的装备被销毁后无法从流水还原 |

### 1.4 用户未明示、按默认值落地的决定(可随时改)

| # | 决定 | 默认值 | 改它要动哪里 |
|---|---|---|---|
| E1 | 强化放大什么 | 只放大这件装备的**基础属性**(武器伤害、防具防御 / 速度);随机属性与宝石不放大 | `player_equip.cpp` 的 `ResolveBaseNow` |
| E2 | 强化上限 | +12(= `EquipEnhance` 表行数) | 只改表 |
| E3 | 强化失败的后果 | 金币与材料照扣,**不掉级**(表列 `fail_level_loss` 全填 0) | 只改表 |
| E4 | 强化消耗 | 金币 + 一种材料(强化石);成败都扣 | 只改表 |
| E5 | 强化 / 镶嵌的对象在哪 | 人物背包里的或身上穿着的都行;仓库 / 临时格里的不行 | `player_equip.cpp` 的 `LocateEquipment` |
| E6 | 孔位怎么来 | 按**部位**给定(武器 2 孔、衣服 2 孔、帽子 / 鞋各 1 孔),每个孔限定宝石类型(0 = 任意) | 只改表 |
| E7 | 孔里已有宝石再嵌 | 拒绝,要先取下(不做「顶替并退回」) | `player_equip.cpp` 的 `InlayGem` |
| E8 | 取下宝石 | 免费,原样退回 1 颗到人物背包;背包放不下就拒绝 | `player_equip.cpp` 的 `RemoveGem` |
| E9 | 同一件装备能否嵌多颗同类宝石 | 能(只受孔位类型限制) | — |
| E10 | 全部数值 | 见 §2,是**占位建议**,未经实测 | 只改表 |
| E11 | 数据落点 | 强化等级与宝石放进 `EquipInstanceData`(随 `ItemComp.equip` / `ItemEntry.equip` 整份搬运),**不启用** `ItemEntry` 预留的 6 号 `enchant_level` / 8 号 `gem_uuids` | 见 §3.1 |
| E12 | 宝石怎么记 | 记宝石的 `config_id`,不记 guid(宝石可叠加,guid 不是稳定身份) | 见 §3.1 |

## 2. 配置表

导表流程见 `data/AGENTS.md`。xlsx 由 `tools/scripts/equip_xlsx_patch.py` 的新子命令生成 / 打补丁(纪律同一期:
多会话共写的 `Item.xlsx` / `Tip.xlsx` 不整表写回,保存后读回核对)。

**不给 `Item` 表加列**:Item 表在战斗配表指纹内,且被所有 Go 进程严格加载;「是不是宝石」「有几个孔」「强化规则」都放在
只有 scene 读的新表里。本次对 Item 表只**加行**(强化石与宝石物品)。

### 2.1 `EquipEnhance`(新表)—— 一行一个强化等级

schema `data/schema/equipenhance_table.proto`(`message EquipEnhanceTable`,sheet `EquipEnhance`,主键 `id`),全标量:

| 号 | 列 | 类型 | 含义 |
|---:|---|---|---|
| 1 | `id` | uint32 | **目标**强化等级(主键,从 1 起连续)。第 N 行 = 「从 +(N−1) 强化到 +N」的规则,以及 +N 的加成 |
| 2 | `bonus_permyriad` | uint32 | 强化到本级后基础属性的**总**加成(万分比,不是相对上一级的增量);随等级单调不减 |
| 3 | `success_rate` | uint32 | 成功率(万分比,1..10000) |
| 4 | `fail_level_loss` | uint32 | 失败时掉几级(0 = 不掉;最多掉到 0) |
| 5 | `cost_gold` | uint64 | 金币消耗(成败都扣);0 = 不收 |
| 6 | `material_id` | uint32 | 材料物品,`[(cfg_fk) = "Item"]`;0 = 不要材料 |
| 7 | `material_count` | uint32 | 材料数量(成败都扣);`material_id == 0` 时必须为 0 |

强化上限 `MaxEnhanceLevel()` = 表里从 1 起**连续**的最大 id(表为空 = 0 = 强化未开放)。

初始 12 行(材料统一 2001 强化石):

| id | bonus_permyriad | success_rate | fail_level_loss | cost_gold | material_count |
|---:|---:|---:|---:|---:|---:|
| 1 | 500 | 10000 | 0 | 1000 | 1 |
| 2 | 1000 | 10000 | 0 | 2000 | 1 |
| 3 | 1500 | 10000 | 0 | 3000 | 2 |
| 4 | 2000 | 9000 | 0 | 5000 | 2 |
| 5 | 2600 | 8000 | 0 | 8000 | 3 |
| 6 | 3200 | 7000 | 0 | 12000 | 3 |
| 7 | 3800 | 6000 | 0 | 18000 | 4 |
| 8 | 4500 | 5000 | 0 | 26000 | 4 |
| 9 | 5200 | 4000 | 0 | 36000 | 5 |
| 10 | 6000 | 3000 | 0 | 50000 | 6 |
| 11 | 7000 | 2500 | 0 | 70000 | 8 |
| 12 | 8000 | 2000 | 0 | 100000 | 10 |

### 2.2 `EquipGem`(新表)—— 一行一种宝石

schema `data/schema/equipgem_table.proto`(`message EquipGemTable`,sheet `EquipGem`,主键 `id`),全标量:

| 号 | 列 | 类型 | 含义 |
|---:|---|---|---|
| 1 | `id` | uint32 | 宝石物品的 `Item.id`(主键,`[(cfg_fk) = "Item"]`)。**「是不是宝石」= 本表有这一行,且对应 Item 行存在、不是装备**(§4.3 `IsGem`) |
| 2 | `gem_type` | uint32 | 宝石类型(≥ 1);孔位按类型接纳 |
| 3 | `attr_id` | uint32 | 提供的属性,`[(cfg_fk) = "EquipAttribute"]`;效果走一期的 `equiprules::ApplyEffect` |
| 4 | `value` | uint32 | 属性值(> 0);百分比属性为整数百分点 |

初始 12 行(类型 1 攻击 / 2 防御 / 3 气血 / 4 迅捷;属性 id 见一期 §2.1):

| id | 名称(Item.name) | gem_type | attr_id | value |
|---:|---|---:|---:|---:|
| 2101 / 2102 / 2103 | 攻击宝石·一级 / 二级 / 三级 | 1 | 1(伤害) | 30 / 60 / 100 |
| 2201 / 2202 / 2203 | 防御宝石·一级 / 二级 / 三级 | 2 | 8(防御) | 12 / 24 / 40 |
| 2301 / 2302 / 2303 | 气血宝石·一级 / 二级 / 三级 | 3 | 9(气血) | 60 / 120 / 200 |
| 2401 / 2402 / 2403 | 迅捷宝石·一级 / 二级 / 三级 | 4 | 11(速度) | 12 / 24 / 40 |

### 2.3 `EquipGemSocket`(新表)—— 一行一个孔位

schema `data/schema/equipgemsocket_table.proto`(`message EquipGemSocketTable`,sheet `EquipGemSocket`,主键 `id`),全标量:

| 号 | 列 | 类型 | 含义 |
|---:|---|---|---|
| 1 | `id` | uint32 | 行号(主键,`equip_kind * 100 + socket`) |
| 2 | `equip_kind` | uint32 | 部位,`[(cfg_index) = true]`;同一部位的全部行 = 该部位装备的孔位 |
| 3 | `socket` | uint32 | 孔位号(0 起,同部位内连续、不重复,< `equiprules::kMaxGemSockets` = 8) |
| 4 | `gem_type` | uint32 | 该孔接纳的宝石类型;0 = 任意类型 |
| 5 | `name` | string | 孔位显示名(下发给客户端;允许留空,客户端空名时显示「孔位 N」) |

孔位是数据(AGENTS §11.6「槽位由配置表枚举」):加孔只加行,不在 C++ 里写 `if 部位 == 武器`。

初始 6 行:

| id | equip_kind | socket | gem_type | name |
|---:|---:|---:|---:|---|
| 1100 | 11(武器) | 0 | 1(攻击) | 攻击孔 |
| 1101 | 11 | 1 | 0(任意) | 通用孔 |
| 1200 | 12(帽子) | 0 | 3(气血) | 气血孔 |
| 1300 | 13(衣服) | 0 | 2(防御) | 防御孔 |
| 1301 | 13 | 1 | 3(气血) | 气血孔 |
| 1400 | 14(鞋子) | 0 | 4(迅捷) | 迅捷孔 |

### 2.4 `Item` 表新行(不加列)

物品 id 段:**2000–2999 = 装备养成材料**(2001 强化石;21xx–24xx 宝石,百位 = 宝石类型,个位 = 等级)。
13 行,全部 `max_stack_size = 999`、`equip_kind = 0`、`battle_usable = 0`,只填 `id / max_stack_size / name / description`,
其余列留空(与一期「留空不填 0」的纪律一致;`equip_kind` / `battle_usable` 两列按既有 15–28 号行的填法):

| id | name | description |
|---:|---|---|
| 2001 | 强化石 | 强化装备时消耗的材料。 |
| 2101–2103 | 攻击宝石·一级 / 二级 / 三级 | 可镶嵌在装备的攻击孔或通用孔,提升伤害。 |
| 2201–2203 | 防御宝石·一级 / 二级 / 三级 | 可镶嵌在装备的防御孔或通用孔,提升防御。 |
| 2301–2303 | 气血宝石·一级 / 二级 / 三级 | 可镶嵌在装备的气血孔或通用孔,提升气血。 |
| 2401–2403 | 迅捷宝石·一级 / 二级 / 三级 | 可镶嵌在装备的迅捷孔或通用孔,提升速度。 |

### 2.5 tip 码(追加在 `//equip_error` 组内,紧跟一期 11 个码之后,顺序如下)

| 码名(表里不带 `k`) | 文案 | fault |
|---|---|---|
| `EquipEnhanceUnavailable` | 该装备无法强化 | |
| `EquipEnhanceMaxLevel` | 已强化到最高等级 | |
| `EquipEnhanceMaterialNotEnough` | 强化材料不足 | |
| `EquipEnhanceGoldNotEnough` | 金币不足,无法强化 | |
| `EquipGemNotGem` | 该物品不是宝石 | |
| `EquipGemNotOwned` | 背包里没有这颗宝石 | |
| `EquipGemSocketInvalid` | 该装备没有这个宝石孔 | |
| `EquipGemSocketOccupied` | 该孔位已镶有宝石,请先取下 | |
| `EquipGemSocketEmpty` | 该孔位没有宝石 | |
| `EquipGemTypeMismatch` | 宝石类型与孔位不符 | |
| `EquipGemBagFull` | 背包已满,无法取下宝石 | |

号由导表器发(发号规则是「已发号按名保留,新名字按表内出现顺序取本段最小空位」,所以**行序就是号序**,预期 28011–28021;
**任何手写文件里都不许出现这些数字**)。一期的 `EquipItemNotFound` / `EquipNotEquipment` / `EquipInBattle` / `EquipFrozen` /
`EquipInternalError` 原样复用。

补丁子命令(建议名 `forge-tip-codes`)的两条路径,幂等判据都是「`equip_error` 组内,这 11 个码紧跟在一期 11 个码之后,
且 (码名, 文案, fault) 逐项一致」:

- (a) `//equip_error` 组最后一个码行之后到表尾全空 → 只写空格(`apply_patches_and_verify`);
- (b) 其后还有别的组(例如别的分支先进 main 的 `//mystery_merchant_error`)→ 在组尾下一行 `ws.insert_rows(at, amount=11)` 再写,
  保存后读回,逐组核对**其它所有组**的 (码名, 文案, fault) 序列不变(照 `tools/scripts/guild_b5a_xlsx_patch.py` 的做法)。

### 2.6 工程登记(手工;导表器不写工程文件)

- `cpp/generated/table/CMakeLists.txt`、`table.vcxproj`、`table.vcxproj.filters` 三处,照一期 `equipaffixpool` / `equipattributecap` 的登记写法各加:
  - `code/equipenhance_table.{h,cpp}`、`code/equipenhance_table_comp.h`、`code/equipenhance_table_fk.{h,cpp}`、`proto/equipenhance_table.pb.{h,cc}`;
  - `code/equipgem_table.{h,cpp}`、`_comp.h`、`_fk.{h,cpp}`、`proto/equipgem_table.pb.{h,cc}`(有外键:`id → Item`、`attr_id → EquipAttribute`);
  - `code/equipgemsocket_table.{h,cpp}`、`_comp.h`、`proto/equipgemsocket_table.pb.{h,cc}`(**没有 `_fk`**:只有索引)。
- `cpp/tests/bag_test/bag_test.vcxproj` 与 `.filters` 加 `bag_remove_exact_test.cpp`(`cpp/tests` 下没有 CMakeLists,只走 vcxproj)。
- `tools/scripts/gen_docs_index.py` 的分类规则认得本文档(`equipment-*`),并重生 `docs/README.md`。

## 3. 协议

### 3.1 实例层(`proto/common/component/item_base_comp.proto`)

```proto
// 一颗已镶嵌的宝石。记 config 不记 guid:宝石是可叠加物品,guid 不是稳定身份。
message EquipGem
{
	uint32 socket = 1;         // 孔位号(0 起)。显式字段,repeated 下标不承载语义(AGENTS §11.6)
	uint32 gem_config_id = 2;  // 宝石的 Item 表 id(= EquipGem 表主键)
}

message EquipInstanceData
{
	repeated EquipAffix affixes = 1;
	uint32 enhance_level = 2;    // 强化等级(+N),0 = 未强化
	repeated EquipGem gems = 3;  // 已镶嵌宝石;同一 socket 至多一条
}
```

`ItemComp.equip`(5)与 `ItemEntry.equip`(14)共用这个消息,`bag_marshal` 是带 presence 的整份 `CopyFrom`,
所以**容器层与存档层零改动**。`proto/common/database/bag_quest_mail_data.proto` 里 `ItemEntry` 的 TODO 表:
6 号 `enchant_level`、8 号 `gem_uuids` 改注为「不再启用,已由字段 14 `equip` 承载,别复用」(同 7 号的写法);
`item_store.h`、`bag_marshal.{h,cpp}` 里说「强化等级 / 镶嵌仍是预留」的注释同步改。

`presence` 语义不变:`has_equip() == true` 表示实例已完成初始化。**强化 / 镶嵌只接受 `has_equip()` 的实例**,
绝不对未初始化的实例调 `mutable_equip()`;对它们的拒绝形态见 §4.3(不是故障码)。

### 3.2 流水类型(`proto/common/rollback/transaction_log.proto`,枚举末尾追加)

```proto
TX_EQUIP_ENHANCE = 27;     // 装备强化:扣金币 + 销毁材料(成败都扣)。correlation_id = 装备 item guid
TX_EQUIP_INLAY = 28;       // 宝石镶嵌:销毁 1 颗宝石物品。correlation_id = 装备 item guid
TX_EQUIP_GEM_REMOVE = 29;  // 取下宝石:发回 1 颗宝石物品。correlation_id = 装备 item guid
```

物品流水的 `extra`(紧凑 JSON,手拼字符串即可):强化扣材料 `{"from":<强化前等级>,"success":true|false}`;
镶嵌 `{"socket":<N>}`;取下 `{"socket":<N>}`。货币流水没有 `extra` 形参,只带 `txType` 与 `correlationId`。

### 3.3 客户端协议(`proto/scene/player_bag.proto`)

```proto
EquipAttrLineInfo 追加:
  uint64 enhance_bonus = 8;       // 仅基础属性行:强化带来的增量,**已含在 value 内**
  uint64 enhance_next_value = 9;  // 仅基础属性行、且该装备带 enhance_next 时:强化到下一级后这一行的 value(预览)

// 装备上的一个宝石孔。
message EquipGemSocketInfo {
  uint32 socket = 1;           // 孔位号
  uint32 gem_type = 2;         // 该孔接纳的宝石类型,0 = 任意
  uint32 gem_config_id = 3;    // 占着这个孔的宝石 config;0 = 空孔。**「是否占用」只看它**
  string gem_name = 4;         // 宝石名(空孔为空;宝石的 Item 行查不到时也为空)
  string gem_icon_key = 5;
  EquipAttrLineInfo attr = 6;  // 宝石**当前生效**的属性。gem_config_id != 0 却不带 attr = 已镶但当前不生效(仍可取下)。tier 恒 0、cap 恒 0、seq 恒 0
  string name = 7;             // 孔位显示名(可为空)
}

// 下一次强化的预览(客户端零配表)。
message EquipEnhancePreview {
  uint32 next_level = 1;
  uint32 success_rate = 2;        // 万分比
  uint64 cost_gold = 3;
  uint32 material_config_id = 4;  // 0 = 不要材料
  string material_name = 5;
  uint32 material_count = 6;
  uint32 bonus_permyriad = 7;     // 升到 next_level 后基础属性的总加成(万分比)
  uint32 fail_level_loss = 8;
  string material_icon_key = 9;
}

BagItemInfo 追加:
  uint32 enhance_level = 14;                     // 装备:当前强化等级(已按表上限夹取)
  uint32 enhance_max_level = 15;                 // 装备:强化上限;0 = 强化未开放
  EquipEnhancePreview enhance_next = 16;         // 装备:下一级预览;已满级 / 未开放 / 实例未初始化 / 下一级的行不可用时不带
  repeated EquipGemSocketInfo gem_sockets = 17;  // 装备:该部位的全部孔位(含空孔),按 socket 升序;实例未初始化时不带
  uint32 gem_type = 18;                          // 宝石物品:宝石类型(≥ 1);不是宝石 = 0
  EquipAttrLineInfo gem_attr = 19;               // 宝石物品:这颗宝石提供的属性(不是宝石、或属性行不可用时不带)。tier / cap / seq 恒 0
  uint32 enhance_bonus_permyriad = 20;           // 装备:当前有效等级的基础属性总加成(万分比);0 级为 0,满级也写

message EnhanceEquipRequest { uint64 item_id = 1; }
message EnhanceEquipResponse {
  TipInfoMessage error_message = 1;
  BagInfo bag = 2;           // 人物背包全量(金币、材料变化)
  BagInfo equipment = 3;     // 装备栏全量
  bool success = 4;          // 本次强化是否成功。**失败不是错误**:error_message 为空、两份快照照常带回
  uint32 enhance_level = 5;  // 操作后的强化等级
}
message InlayGemRequest { uint64 item_id = 1; uint32 socket = 2; uint32 gem_config_id = 3; }
message InlayGemResponse { TipInfoMessage error_message = 1; BagInfo bag = 2; BagInfo equipment = 3; }
message RemoveGemRequest { uint64 item_id = 1; uint32 socket = 2; }
message RemoveGemResponse { TipInfoMessage error_message = 1; BagInfo bag = 2; BagInfo equipment = 3; }

service SceneBagClientPlayer 末尾追加(顺序不可变,CallMethod 按 method index 分发;index 5 / 6 / 7):
  rpc EnhanceEquip (EnhanceEquipRequest) returns (EnhanceEquipResponse);
  rpc InlayGem (InlayGemRequest) returns (InlayGemResponse);
  rpc RemoveGem (RemoveGemRequest) returns (RemoveGemResponse);
```

- 三个响应的 `bag = 2`、`equipment = 3` 与 `EquipItemResponse` 同号同名,handler 复用 `ReplyInventoryAndEquipment` 模板。
- 强化的概率失败**不走 tip**:客户端有 tip 就保留旧快照,而失败时材料已扣,必须带回新快照。
- 镶嵌按宝石的 `config_id` 消耗 1 颗(不指定哪一堆):背包层没有「从指定那一堆扣 k 个并写流水」的入口,也不需要。
- 方法名不以 `Gm` 开头(scene 按前缀拦 GM 指令);请求体无 repeated、无有符号整型(scene 的请求体检查);响应类型名不含 `Empty`。
- **消息号由 proto-gen 发。robot / 客户端 / 测试 / 限流补丁等任何手写文件里都不许出现这三个方法的数字消息号,只引用生成常量**(见 §9)。

## 4. 服务端实现契约

返回值约定同一期:`uint32_t` tip id,**成功是 `kSuccess`(= 1000,不是 0)**,一律写 `err != kSuccess`。

### 4.1 容器编排(`cpp/libs/modules/bag/bag_service.{h,cpp}`)

```cpp
// 按 config 全或无扣除并逐条落销毁流水:任何一种持有量不够就一件不扣,返回 kBagInsufficientItems。
// itemsToRemove 为空,或任一条目 count == 0 → 打 ERROR,返回 kInvalidParameter,一件不扣、不落流水
//   (编程错误:底层对 0 数量的结果取决于玩家是否恰好持有该物品,所以在入口判掉;调用方自己跳过不需要扣的项)。
// 跨 zone 冻结期返回 kInvalidParameter(一件不扣)。不判战斗(D48:闸放在玩法入口层)。
// 扣到 0 的堆会以 size == 0 留在包里,调用方随后调 MergeAndCompact(kMergeOnly) 回收(同战斗结算)。
// 常规的「不够」请调用方先用静默的 Bag::GetTotalItemCount 预检:本函数内部的不足分支会打 ERROR 与调用栈。
// 按 config 抽数量**不看是不是装备**:调用方必须保证被扣的 config 不是装备(§4.3 的写入口在提交前校验)。
static uint32_t RemoveItemsExact(
    entt::entity playerEntity,
    Bag &bag,
    const ItemCountMap &itemsToRemove,
    TransactionType txType,
    uint64_t correlationId = 0,
    const std::string &extra = {});
```

实现照 `RemoveItemsClamped` 的形状:参数检查 → 冻结闸 → `Bag::RemoveItems(itemsToRemove, &drained)`(全或无)→ 每条回执
`LogItemDestroy(playerEntity, guid, configId, amount, correlationId, extra, txType)`(注意 `LogItemDestroy` 的 `txType` 在最后,
与 `LogItemCreate` 的顺序不同)。`modules/bag` 其余文件除注释外零改动(容器层不认识「强化」「宝石」)。

### 4.2 纯规则(`equip_attribute_rules.h`,namespace `equiprules`,只依赖 std)

```cpp
inline constexpr uint32_t kMaxGemSockets = 8;   // 单件装备孔位数上限(表校验用;消费侧也只看实例上前这么多条宝石记录)

// 基础值按万分比放大。bonus == 0 严格恒等;全整数;饱和不回绕。算法写死(实现与用例都照这个):
//   inc    = SaturatingMul(base / 10000, bonus) + (base % 10000) * bonus / 10000   // 第二项最大 9999 * (2^32 - 1),不溢出;两项相加用饱和加
//   result = SaturatingAdd(base, inc)
// 锚点:ScaleByPermyriad(40, 500) == 42;ScaleByPermyriad(12, 500) == 12;ScaleByPermyriad(19999, 5000) == 29998;
//       ScaleByPermyriad(UINT64_MAX, 1) == UINT64_MAX;ScaleByPermyriad(x, 0) == x。
constexpr uint64_t ScaleByPermyriad(uint64_t baseValue, uint32_t bonusPermyriad);

// 强化判定。ratePermyriad == 0:必败且不消耗随机数;其余恰好消耗一次 rand(kPermyriadBase),
// 结果 < ratePermyriad 即成功(ratePermyriad >= kPermyriadBase 必成,但照样消耗一次 —— 与 RollAffixes 的粉 / 黄口径一致)。
template <class RandFn>
bool RollEnhanceSuccess(uint32_t ratePermyriad, RandFn&& rand);

// 强化失败后的等级:level > loss ? level - loss : 0。
constexpr uint32_t LevelAfterFailure(uint32_t level, uint32_t failLevelLoss);

struct GemSocketRule { uint32_t socket{0}; uint32_t gemType{0}; };   // gemType == 0 = 任意
enum class GemFit : uint8_t { kOk = 0, kSocketInvalid = 1, kSocketOccupied = 2, kTypeMismatch = 3, kCount };

// 孔位校验。sockets:该部位的孔位定义;occupied:实例上已占用的孔位号(原始记录,可含重复)。
// 判定顺序写死:① socket 不在 sockets 里 → kSocketInvalid;② socket 在 occupied 里 → kSocketOccupied;
// ③ gemType == 0 || (rule.gemType != 0 && rule.gemType != gemType) → kTypeMismatch;④ kOk。
// sockets 里同一 socket 有多条定义时取第一条。
GemFit CheckGemFit(const std::vector<GemSocketRule>& sockets, const std::vector<uint32_t>& occupied,
                   uint32_t socket, uint32_t gemType);
```

`detail` 里补一个 `constexpr uint64_t SaturatingMul(uint64_t a, uint64_t b)`(与现有 `SaturatingAdd` 并列)。
不往 `AffixTier` / `EffectKind` 里加值(那会放宽存档里随机属性的合法范围,并打破既有 `static_assert`)。

### 4.3 编排(`player_equip.{h,cpp}`,`class PlayerEquipSystem`,新增公开成员)

```cpp
struct EnhanceOutcome { bool success{false}; uint32_t level{0}; };

static uint32_t Enhance(entt::entity player, Guid itemId, EnhanceOutcome& outcome);
static uint32_t InlayGem(entt::entity player, Guid itemId, uint32_t socket, uint32_t gemConfigId);
static uint32_t RemoveGem(entt::entity player, Guid itemId, uint32_t socket);
static uint32_t MaxEnhanceLevel();          // EquipEnhance 表从 1 起连续的最大 id;表空 = 0
static bool IsGem(uint32_t configId);       // EquipGem 表有这一行 && Item 行存在 && equip_kind == 0

// 仅供单测:替换本文件全部掷骰(掷属性与强化判定)用的随机源。rand(n) 返回 [0, n);传空恢复生产随机源(tlsRandom)。
// 进程级静态、无锁:只许在测试的 SetUp / TearDown 里调(同 BagService::SetItemInstanceInitializer 的约定),用完必须复原。
// 没有它,编排层的强化失败分支(照扣、掉级)只能靠概率撞,写不出确定的用例。
static void SetRandomSourceForTest(std::function<uint64_t(uint64_t)> rand);
```

三个写入口都写在 `player_equip.cpp` 里(`CheckWritable` / `OnEquipmentChanged` 是文件内私有函数),骨架同 `Equip`:
**`CheckWritable` → 取 `PlayerBagsComp` → 全部纯校验 → 提交 → 改的是穿着的那件则 `OnEquipmentChanged`**。

**公共:定位装备 `LocateEquipment(bags, itemId)`**(匿名命名空间):先 `bags[kInventory]` 后 `bags[kEquipment]` 按 guid 找,
返回 {所在的 `Bag*`、是否穿着、Item 行、实例是否已初始化(`has_equip()`)}。拒绝码(顺序与 `Equip` 现有写法一致):
找不到 → `kEquipItemNotFound`;Item 行为空 / `equip_kind == 0` / `max_stack_size != 1` → `kEquipNotEquipment`;`size != 1` → `kEquipItemNotFound`。
**未初始化(`!has_equip()`)不在这里拒**:它是一期定义的合法状态(「本来就没有」),由三个入口各自映射成业务码并打 `LOG_WARN`:
强化 → `kEquipEnhanceUnavailable`;镶嵌 → `kEquipGemSocketInvalid`;取下 → `kEquipGemSocketEmpty`(读 `equip()` 的默认实例即可,不必特判)。

**强化 `Enhance`**

| 步 | 动作 | 失败返回 |
|---|---|---|
| 1 | `CheckWritable` | `kEntityIsNull` / `kEquipFrozen` / `kEquipInBattle` |
| 2 | `LocateEquipment`;未初始化 | 见上;`kEquipEnhanceUnavailable` |
| 3 | `max = MaxEnhanceLevel()`;`max == 0` | `kEquipEnhanceUnavailable` |
| 4 | `cur = min(实例.enhance_level, max)`;`cur >= max` | `kEquipEnhanceMaxLevel` |
| 5 | 取 `EquipEnhance` 第 `cur + 1` 行(缺行打 ERROR);`success_rate == 0`(必败的行不收费) | `kEquipInternalError`;`kEquipEnhanceUnavailable` |
| 6 | 要材料(`material_id != 0 && material_count > 0`)时:材料的 Item 行必须存在且 `equip_kind == 0`(否则打 ERROR —— 按 config 扣会把装备当材料销毁) | `kEquipInternalError` |
| 7 | 要材料时 `inventory.GetTotalItemCount(material_id) < material_count` | `kEquipEnhanceMaterialNotEnough` |
| 8 | `cost_gold > 0` 时 `!CurrencySystem::CanAfford(player, kCurrencyGold, cost)`(`cost_gold > INT64_MAX` 视为付不起) | `kEquipEnhanceGoldNotEnough` |
| —— | **以上全是纯校验,任何一条失败什么都没动** | |
| 9 | `success = equiprules::RollEnhanceSuccess(row.success_rate, RandBelow)`(先掷后扣:流水的 `extra` 要带成败;结果在回包前不外露) | — |
| 10 | `cost_gold > 0` 时 `CurrencySystem::DeductCurrency(player, kCurrencyGold, cost, TX_EQUIP_ENHANCE, itemId)` | 打 ERROR,`kEquipInternalError`(什么都没动) |
| 11 | 要材料时 `BagService::RemoveItemsExact(player, inventory, {{material_id, material_count}}, TX_EQUIP_ENHANCE, itemId, extra)` | 打带 `[PlayerEquip][ENHANCE_PARTIAL]` 标签的 ERROR(金币已扣),`kEquipInternalError` |
| 12 | **重新**按 guid 取实例指针,写 `enhance_level = success ? cur + 1 : LevelAfterFailure(cur, row.fail_level_loss)` | 取不到:打 `[PlayerEquip][INSTANCE_LOST]` ERROR,`kEquipInternalError` |
| 13 | 扣过材料则 `BagService::MergeAndCompact(player, inventory, CompactPolicy::kMergeOnly)` 回收空堆(失败只打 ERROR)。**放在写实例之后**:「已扣」与「已写」之间不夹任何会销毁实例的步骤 | — |
| 14 | 等级变了且装备穿着 → `OnEquipmentChanged(player)` | — |
| 15 | `LOG_INFO`(player、guid、config、前后等级、成败);填 `outcome`;`return kSuccess` | — |

提交顺序排成「后面的步骤不会再失败」:scene 单线程、冻结已被第 1 步挡掉、余额与持有量都问过,所以第 10、11 步在正常情况下
不会失败;第 11 步的失败分支只为留下可对账的日志(没有货币退款原语,`AddCurrency` 会被欠款抵扣与封禁闸改写,不能当回滚用)。
金币与材料都不退(成败都扣是设计,不是故障)。

**镶嵌 `InlayGem`**

| 步 | 动作 | 失败返回 |
|---|---|---|
| 1 | `CheckWritable` | 同上 |
| 2 | `LocateEquipment`;未初始化 | 见上;`kEquipGemSocketInvalid` |
| 3 | `!IsGem(gemConfigId)`(表里没有这一行,或它的 Item 行不存在 / 是装备) | `kEquipGemNotGem` |
| 4 | `CheckGemFit(该部位的孔位定义, 实例 gems 里出现过的**全部** socket(原始记录,不按是否生效过滤;只有 `gem_config_id == 0` 的空壳记录视同不存在,见取下一节), socket, gem.gem_type)` | `kEquipGemSocketInvalid` / `kEquipGemSocketOccupied` / `kEquipGemTypeMismatch` |
| 5 | 实例 `gems_size() >= kMaxGemSockets`(干净数据走不到:孔位已被第 4 步判满;走到 = 脏数据,打 ERROR) | `kEquipInternalError` |
| 6 | `inventory.GetTotalItemCount(gemConfigId) < 1` | `kEquipGemNotOwned` |
| —— | **以上全是纯校验** | |
| 7 | `BagService::RemoveItemsExact(player, inventory, {{gemConfigId, 1}}, TX_EQUIP_INLAY, itemId, extra)` | 打 ERROR,`kEquipInternalError`(什么都没动) |
| 8 | **重新**取实例指针,`add_gems()` 写 `{socket, gemConfigId}` | 取不到:打 `[PlayerEquip][GEM_LOST]` ERROR(含 gem config),`kEquipInternalError` |
| 9 | `MergeAndCompact(kMergeOnly)`(放在写实例之后,理由同强化) | — |
| 10 | 装备穿着 → `OnEquipmentChanged`;`LOG_INFO`;`return kSuccess` | — |

**取下 `RemoveGem`**

| 步 | 动作 | 失败返回 |
|---|---|---|
| 1–2 | `CheckWritable`、`LocateEquipment` | 同上 |
| 3 | 实例的 `gems`(原始记录)里找 `socket`,取第一条 `gem_config_id != 0` 的;没有(含未初始化的实例) | `kEquipGemSocketEmpty` |
| 3a | 宝石的 Item 行不存在,或被配成了装备(打 ERROR;前者发放必败,后者退回去会铸出一件掷了属性的装备实例) | `kEquipInternalError`(宝石仍在装备上) |
| 4 | 静默预检:`inventory.IsSpaceInsufficient(1)` 且没有哪一堆同种宝石还塞得下(同 config、不带 `equip` 段、`size < max_stack_size`,判据同容器层的并堆) | `kEquipGemBagFull` |
| 5 | `BagService::AddItems(player, inventory, blockList, {{gemConfig, 1}}, TX_EQUIP_GEM_REMOVE, itemId, extra)`。**先发物,成功后才抹宝石**(照 `PlayerMissionSystem::ClaimReward`) | `kBagItemNotStacked` / `kBagAddItemBagFull` → `kEquipGemBagFull`;其余 → 打 ERROR,`kEquipInternalError`(宝石仍在装备上) |
| 6 | **重新**取实例指针(第 5 步是一次 Insert),从 `gems` 里删掉该 `socket` 的**全部**条目;除退回的那一条之外每删一条打一条 ERROR(socket + gem_config_id:那是脏数据里的重复 / 空壳记录,被无偿清掉了) | 取不到:打 `[PlayerEquip][INSTANCE_LOST]` ERROR,`kEquipInternalError` |
| 7 | 装备穿着 → `OnEquipmentChanged`;取下的那颗当时不计入加成的话补一条 `LOG_WARN`(提交成功之后才打);`LOG_INFO`;`return kSuccess` | — |

取下不要求该孔位仍在 `EquipGemSocket` 表里,也不要求宝石仍在 `EquipGem` 表里(策划删孔 / 删宝石行之后,宝石照样取得下来);
宝石的 Item 行被删、或被配成了装备,则退不回去,宝石留在装备上(fail-closed;在第 3a 步就拒,不等到发放失败 —— 否则背包满的玩家
会先收到「背包已满」,腾出格子后才发现根本退不了)。「发回了但装备上还在」的复制路径不存在:每个 `Bag` 各有自己的
`ItemStore`,人物背包满了是整批拒绝、不淘汰。

**`gem_config_id == 0` 的记录视同不存在**(镶嵌的占用判定、取下、`ResolveGemsNow`、tooltip 四处同口径):协议里 0 就是「空孔」,
这种记录没有东西可退 —— 当成占用的话,服务器说「已占」、客户端显示空孔、取下必败,这个孔对玩家永久作废。它只可能来自外来的
`equip` 段或存档损坏(镶嵌入口自己写不出 0);重新嵌一次即自愈(下次取下时空壳记录一并清掉)。

**消费侧(加成与 tooltip 同口径,只从下面几个函数取值)**

```cpp
// 匿名命名空间,与 ResolveAffixNow 并列:
uint32_t EffectiveEnhanceLevel(const ItemComp& item);            // has_equip() ? min(enhance_level, MaxEnhanceLevel()) : 0
uint32_t EnhanceBonusPermyriad(uint32_t level);                  // level == 0 → 0;否则 EquipEnhance 第 level 行的 bonus_permyriad(缺行返回 0)
uint64_t ResolveBaseNow(uint64_t tableValue, uint32_t bonusPermyriad);   // = equiprules::ScaleByPermyriad
struct ResolvedGem { uint32_t socket; uint32_t gemConfigId; const EquipAttributeTable* attr; uint64_t value; };
std::vector<ResolvedGem> ResolveGemsNow(const ItemComp& item, const ItemTable& row);
```

`ResolveGemsNow` 的现查规则(入包边界整份信任 `equip` 段,所以消费侧必须自己校验)。只看实例上**前 `kMaxGemSockets` 条**记录;
**先按 socket 取第一条,再对这一条判**(第一条不合格则该 socket 整个不计,不回退到同 socket 的后一条):
① `EquipGem` 表有该 `gem_config_id`;② 该 `socket` 在这件装备部位的孔位定义里;③ 宝石属性行存在且效果被规则层接受(`IsEffectAccepted`)。
**不**复核宝石类型与孔位类型(类型是嵌入时的规则;策划事后改孔位类型不让已嵌的宝石失效)。结果按 `socket` 升序。
**读路径静默**:它被 `GetBag`(每件物品一次)与每次属性重算调用,不合格的记录只跳过、不打日志(否则策划删一行就是全服日志洪泛);
写入口(镶嵌 / 取下)碰到这类记录时打 `LOG_WARN`(带 item guid / socket / gem config)。

- `AccumulateWornItem`:基础属性循环里把 `base.base_attr_value()` 换成 `ResolveBaseNow(...)`;随机属性循环之后加一段宝石循环,
  逐条 `equiprules::ApplyEffect(bonus, attr.effect, attr.effect_param, value)`。
- `FillItemDisplay`:
  - 非装备:若 `IsGem(config)`,写 `gem_type`(18);宝石属性行存在且被接受时写 `gem_attr`(19);然后照旧返回。不是宝石则与现状逐位一致。
  - 装备:基础属性行 `value = ResolveBaseNow(表值, 当前万分比)`、`enhance_bonus = value − 表值`;
    写 `enhance_level`(14,有效等级)、`enhance_max_level`(15)、`enhance_bonus_permyriad`(20)。
  - `enhance_next`(16):仅当 `has_equip()`、有效等级 < 上限、且下一级的行存在并 `success_rate > 0`(与 `Enhance` 第 5 步同判据)时写;
    同时给每条基础属性行写 `enhance_next_value = ResolveBaseNow(表值, 下一级万分比)`。材料名 / 图标取材料的 Item 行(查不到留空)。
  - `gem_sockets`(17):仅 `has_equip()` 时写。按孔位定义逐孔下发(含空孔);**「占用」按原始记录**:实例 `gems` 里有该 socket 的记录
    (取第一条)→ `gem_config_id` = 记录里的 config、名称 / 图标取宝石的 Item 行(查不到留空);**「生效」按 `ResolveGemsNow`**:计入加成的才带 `attr`。
    这样服务器判占用的口径(`CheckGemFit` 的 `occupied`)与客户端看到的一致。
- `Recalculate` / `BuildPanel` / 战斗快照**不用改**:强化与宝石都经 `CollectBonus` 进 `EquipBonus`。`RecalcReason` 复用
  `kEquipmentChanged`(按比例保持当前气血 / 法力:强化加血装不回血)。`player_attribute.h` 里 `Recalculate` 的调用约定注释扩成
  「装备栏的内容一变,**或穿着的装备的实例数据一变**」。

**表校验(`ValidateTables` 追加)**。口径同一期:逐条 `++problems` + `LOG_ERROR "[PlayerEquip] 表校验: ..."`,不拒启;
**一行里互不相干的问题各计一条,同一个根因只计一次**。下面每个分号隔开的条件「恰好 +1」(用例按这张表逐项注入坏数据):

- `EquipEnhance`(逐行):`id == 0` 或 `id > MaxEnhanceLevel()`(不在从 1 起的连续前缀里);`success_rate` 不在 1..10000;
  连续前缀内 `bonus_permyriad` 小于上一级;`cost_gold > INT64_MAX`;`material_id != 0` 且 Item 行不存在(该行不再查后两项);
  `material_id != 0`、Item 行存在且 `equip_kind != 0`;`material_id != 0`、Item 行存在且 `battle_usable != 0`
  (强化石不能是战斗道具:战斗结算按 config 扣物,与强化扣材料会在重登窗口里交错);`material_id != 0` 且 `material_count == 0`;
  `material_id == 0` 且 `material_count != 0`。
- `EquipGem`(逐行):Item 行不存在(该行不再查后两项);Item 行存在且 `equip_kind != 0`;Item 行存在且 `battle_usable != 0`;
  `gem_type == 0`;属性行不存在;`value == 0`。**属性行存在但 `!IsEffectAccepted` 不计**(`EquipAttribute` 段已报过,同一根因)。
- `EquipGemSocket`:`equip_kind == 0`(逐行;这样的行不属于任何部位,不参加下面按部位的两项);`socket >= kMaxGemSockets`(逐行);
  同部位内重复的 socket,每多出一行 +1;按部位把**界内**的 socket 去重后不是 `0..n−1`,每个部位 +1 —— 但越界的行已逐行报过,
  它原本该占的那个号留下的缺口不另计:只有「缺口数 > 越界行数」才报(缺口数 = 界内最大号 + 1 − 界内不同号的个数)。
- 三张表都必须已加载(`InstallItemInitializer` 的调用点在表加载完成回调里,无需新增调用点)。

校验只打日志不拒启,且单表热更不重跑,所以**写入口对缺行、以及被配成装备的材料 / 宝石行一律 fail-closed**(上表第 5、6 步与 `IsGem`)。

### 4.4 入口(`player_bag_handler.{h,cpp}`)

三个新方法照 `EquipItem` / `UnequipItem`:参数粗检 → 调 `PlayerEquipSystem` → 失败 `SetTip` → 成功 `ReplyInventoryAndEquipment`。

- `EnhanceEquip`:`item_id == 0` → `kInvalidParameter`;成功后另写 `success` 与 `enhance_level`。
- `InlayGem`:`item_id == 0 || gem_config_id == 0` → `kInvalidParameter`(`socket` 可以是 0)。
- `RemoveGem`:`item_id == 0` → `kInvalidParameter`。

自定义逻辑只写在 `.cpp` 的 `///<<< BEGIN WRITING YOUR CODE` 守护段内;`.h` 的声明与 `case 5 / 6 / 7` 是生成物的手工补丁,
以 proto-gen 重生成为准。不需要动 gate 的 GM 名单(不是 `Gm*` 指令)。

限流:`equip_xlsx_patch.py` 的 `LIMITER_NEW_METHODS` 加 `EnhanceEquip: 10`、`InlayGem: 5`、`RemoveGem: 5`(发号之后才能跑
`message-limiter`)。档位语义:限流器按秒级时间戳、差值 ≥ 2 才出窗,档位 N 实际是「相邻两秒合计 N 个」;强化是唯一会被长时间连点的
装备操作,给 10(≈ 5 次 / 秒)。没进限流表之前吃默认档 3(≈ 1.5 次 / 秒),超限计非法包、累计 50 次被 gate 断线 —— 所以客户端
数据层自带发包间隔(§6.1),且 `message-limiter` 执行之前不拿真实客户端验收强化。

## 5. 校验与不变量

1. 强化等级与宝石只存在于 `EquipInstanceData`;穿脱(Take / PutInstance)、存档与跨区(`bag_marshal`)是整份搬运,不丢、不重置。
   回滚执行尚未实现(`player_rollback_handler.cpp` 的 TODO),实现时必须走 `bag_marshal`。
2. 强化与宝石的数值不入库:入库的只有「+几」和「哪个孔嵌了哪种宝石」;改表立即对存量装备生效(等级按表上限夹取,不洗存档)。
3. 加成与 tooltip 同口径:基础属性(含下一级预览值)只从 `ResolveBaseNow` 取,宝石属性只从 `ResolveGemsNow` 取;不计入加成的宝石不显示属性。
   「孔位是否占用」是另一件事,服务器与客户端都只看原始记录。
4. 0 级严格恒等:`ScaleByPermyriad(x, 0) == x`,未强化、未镶嵌的装备与一期逐位一致(一期全部用例与 robot 冒烟的期望值不变)。
5. 强化 / 镶嵌 / 取下与穿脱共用 `CheckWritable`:战斗中、跨区冻结中拒绝。它们是 scene 侧**第一批玩家可触发的背包扣减入口**,
   战斗闸必须在入口层(D48)。
6. 先校验后提交;提交段没有可失败的常规路径。取下宝石「先发物、后抹除」:任何失败都不会凭空少宝石。
7. 不对 `!has_equip()` 的实例调 `mutable_equip()`;不对非装备写 `equip` 段(否则该实例永远不能并堆)。
8. `ItemStore` 返回的实例指针在下一次 Insert / Erase(含整理)后失效:改实例前必须重新取。
9. 被扣的材料 / 宝石 config 必须不是装备:按 config 抽数量不看 `has_equip()`,会把装备实例当材料销毁。表校验报、写入口挡,两道都要有。
10. 强化的概率失败是成功的响应(无 tip,带两份快照,`success = false`)。
11. 导表后的同批重编要求同一期 §5 不变量 8、9(Item 表加了 13 行,战斗指纹会变;三张新表只在 scene 读,不进指纹)。

## 6. 客户端(Unity,隔离工作树 `E:\work\mmorpg-client-forge`,只做数据层与纯逻辑)

### 6.1 `PlayerFeaturesClient`(数据层)

```csharp
public readonly struct EquipEnhanceOutcome { public ulong ItemId { get; }  public bool Success { get; }  public uint Level { get; } }

public const int GoldCurrencyIndex = 0;                     // 镜像服务器 currency.h 的 kCurrencyGold
public const long ForgeMinIntervalMs = 700;                 // 强化 / 镶嵌 / 取下 两次发包的最小间隔(按默认限流档 3 留:持续连点要 ≥ 667ms)
public Func<long> NowMs { get; set; }                       // 发包间隔用的时钟;传 null 恢复默认的单调时钟。只给用例替换
public BagInfo CharacterBag => _bags[CharacterBagType];     // 人物背包(Bag 是「当前查看的那一类」,可能是仓库)
public EquipEnhanceOutcome? LastEnhanceOutcome { get; private set; }
public event Action<EquipEnhanceOutcome> OnEnhanceResult;
public void EnhanceEquip(ulong itemId);
public void InlayGem(ulong itemId, uint socket, uint gemConfigId);
public void RemoveGem(ulong itemId, uint socket);
```

- 与穿脱共用同一个在途标志(`BusyEquip`)与请求序号(`_equipActionRequest`):现成的互斥、延后补发、旧连接 / 旧角色回包拒绝全部沿用。
  `PerformEquipAction(ulong itemId, bool equip)` 的 `bool equip` 换成「操作名 + 发包委托」;`Complete` 增加第 4 个参数 `Action applied = null`。
  **穿 / 脱两条路径的行为与文案逐字不变**(既有 70 条用例守着)。
- 操作名(用于「{操作名}:{原因}」):强化 `无法强化`;镶嵌 `镶嵌失败`;取下 `取下失败`。(不用「强化失败」:会与掷骰失败混淆。)
- 成功路径顺序固定:覆盖两份快照 → `applied?.Invoke()`(强化在这里写 `LastEnhanceOutcome`)→ `OnChanged`(仍只触发一次)→
  强化再触发 `OnEnhanceResult`(触发前复核 `IsCurrent` 与请求序号)→ `SendDeferredReads`。
- `LastEnhanceOutcome` 的清理点:每次装备类操作(穿 / 脱 / 强化 / 镶嵌 / 取下)开始时、`ClearSnapshots`;`Reject` 路径不写。`Dispose` 把 `OnEnhanceResult` 置空。
- **发包间隔**:三个新操作距离上一次**新操作发包**不足 `ForgeMinIntervalMs` 的点击按连点丢弃(不报错、不发包;闸紧跟在 `BusyEquip`
  之后、互斥提示与本地拒绝之前)。时钟可注入(公开属性 `NowMs`,默认单调时钟;用例里替换 —— 测试程序集没有 `InternalsVisibleTo`,
  所以不是 internal),不改构造函数签名(`GameClient` 那一处调用是别的会话的在途文件)。数据层不提供「冷却中」状态,界面要反馈就按这个常量自己做按钮冷却。
- **发包前的本地拒绝**(不发包、不重拉,文案走 `OnError`):`itemId == 0`;镶嵌时 `gemConfigId == 0`;
  目标不在人物背包与装备栏两份快照里;强化时已满级(`EnhanceMaxLevel > 0 && EnhanceLevel >= EnhanceMaxLevel`)或未开放(`EnhanceMaxLevel == 0`);
  镶嵌时目标孔不存在 / 已占用(`GemConfigId != 0`);取下时目标孔为空;镶嵌时人物背包里该宝石数量为 0。
  金币 / 材料不足**不在本地拦**(快照可能偏旧,拦了会把人锁死),只给模型置灰用。两份快照任一缺失时不做本地拒绝,直接发包。
- 回包整体覆盖两份快照;业务 tip 先看响应体;传输失败绝不自动重发(强化不是幂等操作),只重拉快照。
- 失败后是否重拉(`IsRuleRejection` 扩充;判据不变:「服务器没动过状态,且本地快照没有被证明过期」才不重拉):

  | 不重拉 | 重拉两份快照 |
  |---|---|
  | `KEquipGemTypeMismatch`、`KEquipGemBagFull`,以及既有的 `KEquipNotEquipment` / `KEquipInBattle` / `KEquipFrozen` / 限流 | `KEquipGemNotOwned`、`KEquipGemSocketOccupied`、`KEquipGemSocketEmpty`、`KEquipGemSocketInvalid`、`KEquipGemNotGem`、`KEquipEnhanceMaxLevel`,以及既有的 `KEquipItemNotFound` / `KEquipInternalError` / 未收录的码 |
  | `KEquipEnhanceMaterialNotEnough` / `KEquipEnhanceGoldNotEnough`:服务器给的码 == 发包那一刻 `EquipBagModel.CheckEnhance` 的结论(本来就知道缺这一样)→ 不重拉 | 同两个码:本地以为够,或以为缺的是另一样 → 快照过期,重拉 |
  | `KEquipEnhanceUnavailable`:本地快照本来就没有下一级预览(`CheckEnhance` 得 `Unavailable`:实例未初始化,或下一级是必败的行 —— 服务器不给预览、也照样拒)→ 不重拉 | 同一个码:本地带着预览、以为能强化 → 快照过期,重拉 |

- 文案(`DescribeEquipTip`)逐字镜像 §2.5,只写枚举名不写数字。

### 6.2 `EquipDisplay`(纯函数排版;只管排版与顺序,不推算任何数值)

逐字格式(全角冒号 `：`、全角括号 `（）`;`percent` 的行数值后带 `%`):

| 场景 | 格式 | 例 |
|---|---|---|
| 基础属性行,`EnhanceBonus == 0` | 与现状逐字相同:`{名称}：{值}` | `伤害：40` |
| 基础属性行,`EnhanceBonus > 0` | `{名称}：{值}（+{增量}）` | `伤害：46（+6）` |
| 基础属性行的下一级预览 | `{名称}：{值} → {EnhanceNextValue}` | `伤害：46 → 48` |
| 名称后缀 | `{名称} +{N}`;`N == 0` 不加 | `青铜剑 +3` |
| 空孔 | `{孔名}：未镶嵌` | `攻击孔：未镶嵌` |
| 已镶且生效 | `{孔名}：{宝石名}（{属性名} +{值}）` | `攻击孔：攻击宝石·一级（伤害 +30）` |
| 已镶但不生效(无 `attr`) | `{孔名}：{宝石名}（未生效）` | |
| 孔名 | `Name` 非空用它;空则 `孔位 {socket + 1}` | `孔位 1` |
| 宝石物品说明行 | `{属性名} +{值}` | `伤害 +30` |
| 成功率 | 万分比 / 100,至多两位小数,去掉末尾的 0,加 `%` | `90%`、`12.5%` |
| 强化消耗 | `金币 {cost}`;有材料再加 `、{材料名} ×{count}`;材料名 / 宝石名为空用 `物品 · {config}`(与背包格子上缺名物品的占位一致) | `金币 5000、强化石 ×2` |

`LinesOf` 的签名与返回不变(只含基础 + 随机属性);孔位行另出函数。`EquipAttrTier` 不加新值。

### 6.3 `EquipBagModel`(纯逻辑模型;函数显式接收人物背包快照)

- 孔位视图:一件装备的 `GemSockets` 按 `Socket` 升序。
- 「这个孔能嵌哪些宝石」:人物背包里 `GemType != 0` 且(孔的 `GemType == 0` 或相等)的物品,**按 `ConfigId` 汇总数量**(同一种宝石可能分几堆;请求按 config 扣)。
- 强化前置检查(用于按钮置灰与 §6.1 的重拉判定):返回 `None / Unavailable / MaxLevel / GoldNotEnough / MaterialNotEnough / Unknown`。
  满级 := `EnhanceMaxLevel > 0 && EnhanceLevel >= EnhanceMaxLevel`;未开放 := `EnhanceMaxLevel == 0`;没有 `EnhanceNext`(且不属于前两种)→ `Unavailable`;
  金币读人物背包 `Currency.Values[GoldCurrencyIndex]`,`Currency` 缺失或下标越界 → `Unknown`(不置灰);材料数量按 `ConfigId` 汇总。

### 6.4 用例(`Assets/Tests/EditMode/Battle/`,新文件各配 `.meta`)

`EquipForgeClientTests.cs`(数据层:三个请求的成功 / 各类拒绝 / 重拉分类 / 本地拒绝 / 发包间隔 / 强化结果发布与清理 / 旧连接与旧角色回包 /
与穿脱整理任务的互斥)、`EquipForgeModelTests.cs`(排版逐字、模型)。照 `EquipClientTests.cs` 的夹具写;`FakeBattleTransport` 是 internal,
只能放这个目录。窗口级用例(`EquipWindowTests.cs`)是别的会话的在途文件,不加。

### 6.5 生成物与预期的编译错误

- `tools/gen_messageids.ps1` 白名单(背包段)追加三行:`"SceneBagClientPlayerEnhanceEquip" = "EnhanceEquip"`、
  `"SceneBagClientPlayerInlayGem" = "InlayGem"`、`"SceneBagClientPlayerRemoveGem" = "RemoveGem"`(不在白名单里的方法不出常量、也不告警)。
- `PlayerBag.cs` 由 `tools/gen_proto.ps1 -ProtoRoot <服务器仓>` 重生成(只依赖 proto 源,不依赖发号),本次在隔离工作树里生成好。
- `MessageIds.cs` 的 3 个常量要等服务器 proto-gen 发号;`EquipErrorTip.cs` 的 11 个枚举名要等服务器正式导表。在那之前客户端有
  **预期的 CS0117**(只出现在 `PlayerFeaturesClient.cs` 的 3 处 `_net.Call`、两张 tip switch,以及引用它们的用例),所以这批改动留在隔离工作树,不进客户端 main。
- 界面入口(Codex 的文件,本次不改):`EquipActionsDialog` 的「强化、洗练、镶嵌尚未开放」处接三个新入口;`EquipTooltip` 把 `EquipDisplay` 的新行
  喂进卡片。接线说明见客户端 `Docs/EquipmentForge-20261009.md`。

## 7. 验证

全部**未编译、未运行**;用例条数以 §10 的落码记录与 Codex 实跑为准。

| 层 | 位置 | 内容 |
|---|---|---|
| 纯规则 | `cpp/tests/turn_battle_engine_test/equip_attribute_rules_test.cpp` | `ScaleByPermyriad`(恒等、取整、五个锚点、饱和)、`RollEnhanceSuccess`(0 不耗随机数、边界、满值照掷)、`LevelAfterFailure`、`CheckGemFit`(四种结果与判定顺序、`gemType == 0`) |
| 容器编排 | `cpp/tests/bag_test/bag_remove_exact_test.cpp`(新) | `RemoveItemsExact`:全或无、多 config、空表与 0 数量(持有 / 不持有)、流水类型、冻结、扣到 0 留空堆 |
| 实例层 | `cpp/tests/bag_test/bag_instance_data_test.cpp` | 新字段随搬运 / 存档往返(含 presence);`ExpectSameEquip` 覆盖新字段 |
| 装备编排 | `cpp/tests/bag_test/player_equip_test.cpp` | 三张新表的校验(逐项 +1)、强化(成功 / 失败 / 掉级 / 各拒绝码 / 扣费对账 / 空堆回收 / 材料被配成装备)、镶嵌与取下(各拒绝码 / 恰好扣 1 / 先发后抹 / 脏数据)、加成汇总、tooltip 投影(占用与生效两个口径)、handler、method index |
| 属性接入 | `cpp/tests/bag_test/player_equip_attribute_test.cpp` | 穿着时强化 / 镶嵌 / 取下的六项增量、当前气血按比例、存档往返后重算 |
| GM 闸名单 | `cpp/tests/currency_test/client_gm_gate_test.cpp` | 三个新方法进「正常 RPC」名单 |
| 存档 / 跨区一跳 | `cpp/tests/bag_test/player_feature_persistence_test.cpp`、`cpp/tests/cross_zone_test/cross_zone_test.cpp` | 既有的两条 `EquipInstanceData…` 往返用例里,实例带上强化等级与宝石(孔位号故意不等于下标) |
| 端到端 | `robot/robot.exe -c etc/equip_forge_smoke.yaml`(账号 `robot_9104`) | 见下 |
| 客户端 | `Assets/Tests/EditMode/Battle/EquipForgeClientTests.cs`、`EquipForgeModelTests.cs` | 见 §6.4 |

**robot 冒烟 `equip-forge-smoke`**(新 mode;改 `robot/config/config.go` 白名单与报错串、`robot/main.go` 分发、3 个 handler 桩、
`robot/logic/handler/player_features_response.go` 的两个 switch;新文件复用 `equipSmokeSession` 的 `call / getBag / gmGrant / relogin`
与既有 tip 变量,新增的包级标识符一律带 `equipForge` 前缀 —— 同在 `package main`,重名即编不过):

0. 预备:装备全卸;目标 = `item_id` 最小且 `enhance_level < enhance_max_level` 的 1101,没有就发一把(空位不足判红并提示清号);
   目标各孔的宝石先全部取下。金币与材料「缺多少补多少」(`GmAddCurrency` / `GmGrantItem`,单次上限 99 够一级所需)。
1. 显示:目标带 `enhance_max_level == 12`、`gem_sockets` 两个孔(类型 1 / 0、名称非空)、`enhance_next.next_level == enhance_level + 1`;
   背包里的 2101 带 `gem_type == 1` 与 `gem_attr`。
2. 拒绝码(只测能确定造出来的,每条之后断言两份快照不变):`GemNotGem`(`gem_config_id = 2001`)、`GemSocketInvalid`(`socket = 7`)、
   `GemTypeMismatch`(0 号孔嵌 2201)、`GemNotOwned`(0 号孔嵌从不发放的 2103)、`GemSocketEmpty`;`EnhanceEquip(0)` / `InlayGem(…, 0)` 的参数错误。
3. 强化(在背包里强化一次、穿上后再强化一次):对账**以强化前那份快照里的 `enhance_next` 为准** —— 金币差 = `cost_gold`
   (读 `bag.currency.values[0]`)、材料差 = `material_count`、成功则等级 +1、失败则 `max(0, level − fail_level_loss)`;
   `success_rate == 10000` 时必须成功,其余两种结果都接受。基础属性断言 `value − enhance_bonus == 表值`、
   `enhance_bonus == 表值 × enhance_bonus_permyriad / 10000`(向下取整)。只有「目标穿在身上且等级变了」才等属性面板推送。
4. 镶嵌:0 号孔嵌 2101 → 该孔 `gem_config_id == 2101` 且带 `attr`、背包里 2101 少 1;再嵌同孔 → `GemSocketOccupied`;穿着时面板物伤 / 法伤各 + 宝石值。
5. 重登:强化等级与宝石都在。
6. 取下:该孔变空、2101 回到原数量、面板回到镶嵌前。`EQUIP_FORGE_SMOKE_OK`。

每份响应快照都断言没有 `count == 0` 的条目(空堆必须已回收)。同一种宝石的取下次数远小于 50(见 §8 的异常告警阈值)。
强化不可逆且 robot 没有删物手段:目标到 +12 之后下一轮会自动换一把新的 1101;`EnhanceMaxLevel` / `EnhanceUnavailable` / `GemBagFull`
机器人造不出来,由 C++ 单测覆盖。纯校验函数配 `robot/equip_forge_smoke_scenario_test.go`。

## 8. 已知缺口

- 洗练 / 宝石合成 / 打孔 / 强化保护未做(§1.3)。
- 强化石与宝石没有任何产出途径(掉落 / 任务 / 商店都没配),目前只能 GM 发物;装备本身同样(一期遗留)。
- 流水不带实例数据:强化等级与宝石无法从流水还原。按 guid 扣出(`RemoveItemsByGuid`,寄售托管将来靠它)、临时格淘汰、还原落位失败的丢弃
  三条路径都只留 guid / config / size —— 二期之后它们丢的是 +N 与宝石;接寄售托管之前必须让扣出回执带整份 `ItemComp`。
- 宝石的 `item_uuid` 血缘在镶嵌处断开(销毁 G1、取下铸出 G2),两条流水靠「装备 guid + 宝石 config + extra 里的 socket」配对。
- 没有货币退款原语:强化第 11 步(扣材料)若在金币已扣之后失败,只留 `[ENHANCE_PARTIAL]` 日志供人工对账。正常路径走不到。
- 强化不是幂等操作:客户端超时后玩家再点一次会再扣一次。客户端的约定是传输失败只重拉快照、不自动重发。
- 取下免费且走正式发放入口:同一种宝石 10 分钟内取下第 51 次起,每次都会产生一条 item 类异常获得告警(`AnomalyDetector` 默认阈值
  50 次 / 600 秒,无冷却);「嵌 → 取」循环的流水量只受限流与客户端发包间隔约束。要根治就给取下加成本(表列)。
- 宝石 config 被 GM 封禁时取下会失败(`AddItems` 过发放封禁闸,回 `kInvalidParameter`),对玩家表现为 `kEquipInternalError`,且每次点击各打一条 WARN + ERROR。
- 「战斗中禁写」只认 `InBattleComp`:一期 §8 记的重登 / 离线结算窗口同样适用于强化与镶嵌;登录时 Redis 未就绪的话,窗口持续到下次进场。
  核过没有资产复制路径(强化石 / 宝石不是战斗道具,由表校验守着;金币与背包同一条记录落盘)。仍计划在 `PlayerBattleSystem::IsInBattle` 一处收口。
- 入包边界整份信任 `equip` 段(一期 §8):消费侧已对强化等级夹取、对宝石现查并限制遍历条数,但做邮件回流 / 寄售之前仍须在玩法层补入包校验。
- 策划删掉某部位的孔位定义后,该孔里已嵌的宝石不再计入加成、tooltip 也不显示这个孔(仍可用 `RemoveGem` 按孔位号取下,但界面上看不到入口)。
  孔还在而宝石行被删 / 属性行不可用时,孔显示为「已镶但未生效」,可以取下。
- `kEquipInBattle` / `kEquipFrozen` 的文案是「…无法更换装备」,强化 / 镶嵌复用时措辞不完全贴切。
- 配表热更后在线玩家要到下一次重算才生效(一期遗留)。
- **Java 版(AGENTS §12):待做。** 本机没有 Java 仓库,`PARITY.md` 未登记;装备系统在 Java 版整体未做。做的时候按本文 §2 / §3 对齐客户端可见契约
  (`player_bag.proto` 新字段与 3 个新 RPC、`equip_error` 新 11 个码、3 张新表与 Item 新行)。

## 9. 交接给 Codex 的验证清单(AGENTS §10.1)

> **2026-10-13 更新:本节的步骤已在 main 上实际执行,结果与偏差见 §11。** 下面保留的是落码当天(10-10)的原文,
> 其中的工作树路径、基线提交号、「谁后进 main 谁重发号」等前置都已成为历史(三个新消息号最终是 250 / 251 / 252,见 §11.1)。

**(10-10 原文)现状:服务器侧全部未编译、未正式导表、未 proto-gen、未运行**(Claude 只做过一次写到临时目录的沙盒导表与静态自查,见 §10.8)。
本节是一期文档 §9 的**增量与覆盖**:两期的表与协议要在**同一次**正式导表 / 全量 proto-gen 里首次产出,顺序仍按一期 §9 的 0–11 步;
一期 §9 里的数字(子命令个数、表行数、tip 号范围、用例条数、客户端路径)凡与本节不同的,以本节为准。

### 9.0 代码现在在哪

- 服务器:隔离工作树 `E:\work\xuanming-server-mmo-equip`,分支 `feat/equip-enhance-inlay`,基线 `b3889a7c11`。
  它没有子模块、没有生成物,**不能在这里做正式导表 / proto-gen / 编译**:先并入主仓 `E:\work\xuanming-server-mmo` 的 main,在主仓做。
  本机 main 在基线之后又前进了(含 `ce924341d3`,它把 `proto/message_id.txt` 的 244–249 提交进了本机 main;origin/main `aae0031b7a`
  仍止于 243)。并入时文本上不冲突:`tools/scripts/gen_docs_index.py`、`docs/README.md`、`cpp/tests/cross_zone_test/cross_zone_test.cpp`
  两边改的是不同位置。
- 客户端:隔离工作树 `E:\work\mmorpg-client-forge`,分支 `feat/equip-enhance-inlay`(基于客户端 main `19115d4d`,稀疏检出)。
  生成物到位之前**不要合进客户端 main**(见第 10 步)。

### 9.1 发号与合并前置(现在就定死)

1. 全量 proto-gen 只在「`proto/message_id.txt` 已含 244–249」的树上跑。跑前机械断言三行存在,缺任何一行就中止:
   `246=SceneBagClientPlayerUnequipItem`、`247=SceneBagClientPlayerGmGrantItem`、`249=SceneBagClientPlayerEquipItem`
   (客户端 main 已按 249 / 246 提交;本分支的 `message_id.txt` 止于 243,并入本机 main 之后才有这六行)。
2. 跑后核对:既有行逐行不变;`SceneBagClientPlayerEnhanceEquip / InlayGem / RemoveGem` 三个新号只出现在其后。
   **任何手写文件(robot / 客户端 / 测试 / 限流补丁)里都没有这三个号的数字**,只引用生成常量,所以号是多少都不用改代码。
3. 与 `feat/mystery-merchant`(未提交地预留了 250–252)的约定:谁后进 main,谁在合并后的 main 上重发号,并重跑
   `equip_xlsx_patch.py message-limiter` 与客户端 `gen_messageids.ps1`。
4. `Tip.xlsx` / `Item.xlsx` / `MessageLimiter.xlsx` 合并冲突时取对方版本,再在合并结果上重跑本方的补丁子命令(幂等),不手工合。
   若对方在 `//equip_error` 之后新开了组,`forge-tip-codes` 会走「插入 11 行」的路径并逐格、逐组核对(这条路径只在副本上演练过,
   真跑后看一眼它打印的「其余 N 个组逐项一致」)。它若以「一期 11 个码之后已经有别的码」或「发号 state … 不是从段首连续」中止,
   不要绕过,把输出原样带回。
5. 三处表工程登记(`cpp/generated/table/CMakeLists.txt`、`table.vcxproj`、`.filters`)与别的分支冲突时,两边条目都保留
   (顺序无所谓,三处集合一致即可);`gen_docs_index.py` 的规则行两边都保留后重跑 `PYTHONIOENCODING=utf-8 python tools/scripts/gen_docs_index.py`。
6. 一期 §9 第 2 步的两个配置副本,客户端路径都改指 `mmorpg-client-forge`(不是 `mmorpg-client-equip`,那是停在旧提交的游离检出)。

### 9.2 各步的增量与通过标准

| 一期 §9 的步 | 二期多出来的 / 被覆盖的 | 通过标准 |
|---|---|---|
| 1 xlsx 补丁确认 | 子命令从 5 个变 8 个:再加 `forge-tables`、`forge-item-rows`、`forge-tip-codes` | 八次 `--dry-run` 都报「已是目标状态」。此时**不要**跑 `message-limiter` |
| 1 之后(新增)proto 语法预检 | 四个手改的 proto 没被 protoc 编过。在主仓根目录:`third_party\grpc\install_vs2026_dbg\bin\protoc.exe -I . --descriptor_set_out=%TEMP%\forge_proto_check.pb proto/scene/player_bag.proto proto/common/component/item_base_comp.proto proto/common/rollback/transaction_log.proto proto/common/database/bag_quest_mail_data.proto` | 退出码 0、无输出 |
| 3 导表 | 三张新表 + Item 13 行 + tip 11 个码 | `equipenhance.json` 12 行、`equipgem.json` 12 行、`equipgemsocket.json` 6 行;`item.json` 61 行(带 name 的 33 行);表数比并入前多 3 张(本分支基线上 manifest 是 42);`cpp/generated/table/` 多 19 个文件(`equipenhance` / `equipgem` 各 `.h .cpp _comp.h _fk.h _fk.cpp`,`equipgemsocket` 只有 `.h .cpp _comp.h`,外加 3 对 `.pb.h/.pb.cc`),与工程登记一一对应;`all_table.cpp` 多加载 3 张;`equip_error` 共 22 个码 28000–28021,新 11 个紧跟 `kEquipInternalError`;`tip_enum_ids.json` 只增不改。随后 `gen_schema_index.py` 重生 `data/AGENTS.md` 索引 |
| 4 全量 proto-gen | 见 §9.1 第 1、2 条 | `player_bag_handler.cpp` 无 diff(六个新方法的守护段都在);`.h` 的 case 2–7 与手写一致(有差异以生成的为准并告知);`player_bag_service_metadata.h` 比 proto-gen 前多 6 组常量(一期 3 + 二期 3);`robot/generated/pb/game/message_id.go` 与 `message_body_handler.go` 各多 6 条,六个手写桩没被覆盖 |
| 5 限流表 | `LIMITER_NEW_METHODS` 多了 `EnhanceEquip: 10`、`InlayGem: 5`、`RemoveGem: 5` | `message-limiter --dry-run` 列出 EquipItem / UnequipItem / GmGrantItem(5)、EnhanceEquip(10)、InlayGem / RemoveGem(5),以及缺失时的 GetBag(10)/ SortBag(5);真跑后再导一次表,`messagelimiter.json` 查得到这些号(不要核对总行数:origin/main 另加过 5 行) |
| 6 C++ 编译 | 工程顺序不变(table → proto → core → rpc → grpc_client → modules → battle → scene → 节点 → 测试工程),一律 `/m:1`;`modules` 与 `scene` 必须同批重编(`ItemComp` 布局又变了) | 0 error。scene / modules / 节点工程是 `/W3` + 警告即错误(已关 4244 / 4267 / 4018);`bag_test`、`turn_battle_engine_test` 不是警告即错误 |
| 7 C++ 单测 | 见下表 | 见下表 |
| 8 Go / robot | robot 多 1 个 mode、3 个手写桩、18 个 `TestEquipForge*` | 见 §9.4 |
| 9 冒烟 | 先跑一期 `etc/equip_smoke.yaml`(确认 0 级恒等没被二期带坏),再连跑两遍 `etc/equip_forge_smoke.yaml` | 见 §9.4 |
| 10 客户端 | 工作树换成 `mmorpg-client-forge` | 见 §9.5 |

**第 7 步的硬前置**:读表的测试(`bag_test.exe` 等)在表文件缺失时不是「一条用例红」,而是 `File2String` 的 `LOG_FATAL` 让**整个进程 abort**
(后面几十个套件一起消失,也没有 gtest 汇总)。跑之前先确认 `equipenhance / equipgem / equipgemsocket`(连同一期的 `equipattribute*` 等)
的表文件已经在 `bin/etc/base_deploy_config.yaml` 的 `table_data_directory` 下;日志关键字 ` No such file or directory `。

| exe | 过滤器 | 通过标准 |
|---|---|---|
| `build\cpp\tests\turn_battle_engine_test.exe` | `Equip*` | 56 条(一期 33 + 二期 23:`EquipEnhanceRulesTest` 15、`EquipGemFitTest` 8) |
| `build\cpp\tests\bag_test.exe` | `PlayerEquipTest.*` | 151 条(一期 47 + 二期 104)。分组:`Enhance*` 30、`InlayGem*` 18、`RemoveGem*` 18、`Tooltip*` 14、`CollectBonus*` 12、`*Handler*` 7 |
| 同上 | `PlayerEquipAttributeTest.*` | 37 条(一期 20 + 二期 17) |
| 同上 | `BagInstanceDataTest.*` | 31 条(一期 27 + 二期 4) |
| 同上 | `BagRemoveExactTest.*` | 10 条 |
| 同上 | `PlayerFeaturePersistenceTest.EquipInstanceDataRoundTripsThroughDatabaseRecord` | 绿(实例现在带强化等级与宝石) |
| 同上 | 不带过滤 | 没有既有用例变红 |
| `build\cpp\tests\currency_test.exe` | `ClientGmGateTest.*` | 6 条 |
| `build\cpp\tests\cross_zone_test.exe` | `CrossZoneBagMarshal.*` | 全绿(`EquipInstanceDataSurvivesThePlayerAllDataHop` 现在带强化等级与宝石) |

退出码非 0 却没有 `[  FAILED  ]` 行 = 中途 `LOG_FATAL`,不算绿。各拒绝用例会按设计打 WARN / ERROR 与调用栈(未初始化实例、脏数据、
`[ENHANCE_PARTIAL]` 等),不是失败。`ShippedTablesPassValidation` 红 → 看同次输出里 `[PlayerEquip] 表校验:` 的 ERROR 行,是表的问题,不改测试。
失败时保留:首个编译错误连同上下 20 行;失败用例从 `[ RUN ]` 到 `[  FAILED  ]` 的完整输出。**不要为了让它过而注释断言或跳过用例。**

### 9.3 最可能先出错的位置(按可能性)

1. 生成物的名字:三张表管理器(`EquipEnhanceTableManager` 等,头文件 `table/code/equipenhance_table.h` …)、`GetByEquipKind`、行的 setter、
   11 个 tip 枚举、proto 新字段的访问器。产品代码里的这些名字已对着沙盒导表产物逐个核过;测试夹具里 `EditableEnhanceIndex` 用到的
   `EquipEnhanceTableManager::IdMapType` / `GetIdMap()` 是按既有生成头的规律推的。
2. `player_equip.cpp`(约 +950 行,从未编译):`ResolveGemsNow` 里 `std::min<int>(gems.size(), …)`;`RemoveGem` 里
   `gemsOnInstance->DeleteSubrange(i, 1)`;`ValidateTables` 末尾按部位统计孔位那一段;`Enhance` 里拼 `extra` 的字符串连接;
   `HasStackWithRoom` 传给 `Bag::ForEachItem` 的 lambda。
3. `equip_attribute_rules.h` 里的 6 条 `static_assert`(消息以 `ScaleByPermyriad anchor:` 开头):失败 = 算法与 §4.2 不一致或 MSVC 对
   constexpr 求值有意见。它会让 scene / bag_test / turn_battle_engine_test 同时失败。**不要改断言**,保留完整报错。
4. 三个从未编译的大测试文件(`player_equip_test.cpp` 二期 +2600 行、`player_equip_attribute_test.cpp`、`bag_remove_exact_test.cpp`)。
5. 一期基线本身也从未编译过:首次编译的报错可能落在一期的行上(`Equip` / `Unequip` 的搬运与回滚、`Bag::TakeInstance` / `PutInstance` 等),
   按文件与行号区分是哪一期的。
6. 运行期:每条 `PlayerEquipTest` 的 `SetUp` 里有 `RequireForgeRows`(表没导、或策划改了夹具依赖的行,150 多条会一起红在前置上,信息里带
   「run the table exporter first」);`PlayerEquipAttributeTest` 的面板推送探针靠数 `player_message_utils.cpp` 里一条 WARN 的原文
   (`Player node info not found for player entity`),那句措辞一改,5 条用例红在校准断言上 —— 是探针失效,不是被测代码的问题。

### 9.4 Go / robot 与冒烟

- 构建与单测(工作目录 `robot\`;Go 1.26.5 不在 PATH):
  `$env:GOTOOLCHAIN='local'; $go='C:\Users\luyua\go\pkg\mod\golang.org\toolchain@v0.0.1-go1.26.5.windows-amd64\bin\go.exe'`
  → `& $go mod vendor` → `& $go build -mod=vendor -o robot.exe .` → `& $go vet -mod=vendor . ./logic/handler`
  → `& $go test -mod=vendor -run "TestEquip|TestFeature" . ./logic/handler ./logic/gameobject`。
  通过标准:全部退出码 0;`package main` 里 `TestEquip*` 顶层用例 32 个(一期 14 + 二期 18 个 `TestEquipForge*`);
  `TestEquipFeatureBodiesRejectZeroResponsesAndSurfaceBodyTips` 与 `TestEquipFeatureMessagesHonorEnvelopeErrorsAndKeepTheirSource`
  各 6 个子用例;既有 `TestFeature*` 不回归。Go 服务(login / scene_manager / player_locator / guild)按一期 §9 第 8 步用同批生成物重编。
- 起栈:用同一批产物重启 Go 服务与 scene / gate / battle(scene 与 battle 同批换表;GM 闸为 dev)。先跑 login-test 基线,
  再跑一期 `.\robot.exe -c etc/equip_smoke.yaml`(`robot_9103`,仍以 `EQUIP_SMOKE_OK` 结尾)。
- 二期冒烟(工作目录 `robot\`):`.\robot.exe -c etc/equip_forge_smoke.yaml`,**连跑两遍**(账号 `robot_9104`,不要与 9103 混用)。
  通过标准:两遍都以一行 `EQUIP_FORGE_SMOKE_OK …` 结尾、退出码 0。全新账号的第一遍应为 `granted_weapon=true`、
  `enhance_level=0->1->2`、两次强化都是 `success`(表里 1–3 级成功率 10000);第二遍 `granted_weapon=false`、目标与第一遍相同、
  `enhance_level=2->3->(3 或 4)`(第 3 级必成;第 4 级 9000,成败都算过)。两遍的 `physical_attack` 三段里第一段与第三段相等
  (取下后回到镶嵌前)。scene 启动日志里「装备表校验问题数」为 0。
- 失败时保留:`EQUIP_FORGE_SMOKE_FAIL step=… reason=…` 那一行及其前 20 行(含紧挨着的 `gate envelope error message_id=… tip=…`);
  同时段 scene 日志的 `[PlayerEquip]` 行与全部 ERROR(重点 `[ENHANCE_PARTIAL]` / `[INSTANCE_LOST]` / `[GEM_LOST]` / `表校验`)。
- 按 step 判读:`prep-target`(enhance_max_level = 0)/ `prep-grant-*` / `display-target` → 三张新表或 Item 新行没导、scene 不是同批产物;
  「等响应超时」→ scene / gate 是旧的,或桩被覆盖成空壳;「回了全零 / 残缺的响应」→ gate 级拒绝(限流 / GM 闸);
  `reject-*` → 拒绝码或判定顺序与 §4.3 不一致;`enhance-*-reconcile` → 实扣与预览不符、等级没写、动了别的字段;
  快照里有 `count = 0` → 扣到 0 的空堆没回收;`*-panel` → 改了穿着的装备却没推面板;`relogin-restored` → 强化等级或宝石没随存档往返。
- **`message-limiter` 执行之前不要拿真实客户端连点强化**:这三个方法吃默认限流档(相邻两秒合计 3 个),超限计非法包,累计 50 次被 gate 断线。
  客户端数据层的 700 毫秒发包间隔只是兜底。

### 9.5 客户端(`E:\work\mmorpg-client-forge`)

- 现状:运行时程序集有**预期的 19 处 CS0117,全部在 `PlayerFeaturesClient.cs`**(3 个消息号常量;`DescribeEquipTip` 11 处;`IsRuleRejection` 5 处);
  测试程序集里只有 `EquipForgeClientTests.cs` 引用这些符号。这两份文件之外的 CS0117、或别的错误号,都不是预期。
- 消除步骤:① 服务器正式导表之后:`pwsh -File tools/gen_proto.ps1 -ProtoRoot <已导表的服务器仓> -Protoc E:\work\xuanming-server-mmo\third_party\grpc\install_vs2026_dbg\bin\protoc.exe`
  → `EquipErrorTip.cs` 多出 11 个枚举名;`PlayerBag.cs` 除换行符外无差异(它已在隔离工作树里用同一份 proto 生成好)。
  ② 服务器发号之后:`pwsh -File tools/gen_messageids.ps1 -ProtoRoot <已发号的服务器仓>` → `MessageIds.cs` 出现 `EnhanceEquip / InlayGem / RemoveGem`
  (白名单已加;Warning 的 missing 列表里若有这三个方法,说明还没发号,不要手写数字)。两条脚本都必须带 `-ProtoRoot`。
- 之后:`pwsh -File tools/client_compile_check.ps1`(要在完整检出里跑)→ 0 error;Unity EditMode(`MmorpgClient.Tests.EditMode.Battle`,不带 `-quit`):
  `EquipForgeClientTests` 95、`EquipForgeModelTests` 35 全过;`EquipClientTests` 70、`EquipBagModelTests` 13、`PlayerFeaturesClientTests` 31 不回归。
- 已有的证据(Claude 在临时目录做的离线验证,不是 Unity):用 3 个占位消息号 + 沙盒导表得到的 11 个 tip 枚举名,Roslyn(`-langversion:9.0`)
  编过运行时与 Battle 测试程序集,NUnit 跑通 **244 / 244**;另做过一轮变异测试(44 个变异体全部被用例抓到)。窗口级用例与界面没有跑过。
- 界面接线(Codex 的文件):见客户端 `Docs/EquipmentForge-20261009.md`。

## 10. 实现偏离记录

实现与 §2–§6 不一致、或 §2–§6 没写明而落地时补上的决定。评审后被采纳的修订已并入正文,这里只记要点与出处。

### 10.1 配置表、协议、补丁脚本(§2、§3)

1. `forge-tip-codes` 比 §2.5 多两条「先核对再动手」的中止条件:`//equip_error` 组在一期 11 个码之后已有别的(非本批)码;
   按发号规则推算两期 22 个码的号不是从段首连续。新码的号由行序决定,这两种现状下照写不会报错、但号会错位。
2. Item 新 13 行只写了 `id / max_stack_size / equip_kind(0) / battle_usable(0) / name / description` 六格;`battle_heal_hp / battle_heal_mp` 留空
   (既有 15–28 号行这两格填的是 0)。结果是 `item.json` 里这 13 行没有这两个键,语义等同 0。
3. 一期 `cmd_new_tables` 的循环体原样抽成 `create_scalar_tables`(`new-tables` 与 `forge-tables` 共用);`tip_parse` 的返回多了 `code_rows`。
   一期五个子命令的输出与行为未变(改动前后各 `--dry-run` 一次)。
4. proto 的字段号 / 字段名 / 类型 / rpc 顺序与 §3 一致;长注释按所在文件的风格改成字段上方的前导注释。
   `bag_quest_mail_data.proto` 文件头那段英文说明同步补了一段「6 / 7 / 8 号不再启用」。
5. Claude 做过一次**沙盒导表**(产物只写到会话临时目录,不算正式导表):退出码 0;三张新表 12 / 12 / 6 行、`item.json` 61 行、
   `equip_error` 28000–28021 连续、三处工程登记与实际产出的文件集一一对上。

### 10.2 背包容器编排(§4.1)

1. `RemoveItemsExact` 的用例只断言流水的**条数**(用 txlog 号段计数),没有断言 `tx_type / correlation_id / extra` 的值:
   `TransactionLogSystem` 在单测里没有可读侧(直接交给 Kafka 生产者)。参数顺序靠类型系统守着;「调用方传错 txType 的值」单测发现不了。
2. 多改了两句注释:`bag_service.h` 带整份 `ItemComp` 的 `AddItems` 的信任边界(二期字段同样原样入库不校验);`bag_marshal.cpp` 的
   `ItemToEntry`(新字段随整份 `CopyFrom` 走,别改成逐字段拷)。

### 10.3 纯规则(§4.2)

1. `CheckGemFit` 的定义带 `inline`(头文件里的非模板函数);另加 6 条 `static_assert`(五个锚点 + `GemFit::kCount == 4`)。
2. `RollEnhanceSuccess` 对不守约的随机源(返回值 ≥ n)按 `detail::RollBelow` 的既有口径夹成 n − 1,即除非必成否则算失败。

### 10.4 编排与入口(§4.3、§4.4)

1. `EnhanceOutcome` 是 `PlayerEquipSystem` 的嵌套类型;`Enhance` 开头重置 `outcome`。
2. `ReplyInventoryAndEquipment` 模板的返回类型由 `void` 改为 `bool`(两份快照是否都带上);`EnhanceEquip` 只在它返回 true 时写
   `success / enhance_level` —— 否则快照建不出来的响应会同时带着 tip 与 `success = true`。穿脱两个既有调用点不看返回值,行为不变。
3. 提交点写实例之前的「重新按 guid 取」统一收进 `RefetchInitializedInstance`,它同时复核 `has_equip()`(不变量 7 在提交点也成立)。
4. `[PlayerEquip][ENHANCE_PARTIAL]` 一律带标签,用 `gold_deducted=<实际已扣数>` 说明有没有要对账的金币(该级 `cost_gold` 可以是 0)。
5. `enhance_next` 里的材料字段只在「要材料」(与 `Enhance` 同判据)时才写:`material_id != 0` 但 `material_count == 0` 的脏行按「不要材料」下发。
6. 孔位定义里同一个 socket 有多行(脏表)时只下发排在前面的那一行(与 `CheckGemFit` 取第一条一致);`GetByEquipKind` 的行序 = 表内行序
   (对沙盒生成物核过)。
7. 写入口的 `LOG_WARN` 落在两个点:镶嵌被拒为「已占用」且占着孔的那条记录当前不生效;取下的那条记录当前不生效(照常放行,提交成功后才打)。
8. 表校验:`EquipGem` 的属性行存在但 effect 不被接受时既不计数也不打日志(`EquipAttribute` 段已为那一行打过 ERROR,保持「一条 ERROR 对应一次计数」)。
9. **代码评审后的修订**(都已并入 §4.3 正文):取下新增第 3a 步(宝石的 Item 行不存在或是装备 → 在纯校验段拒);第 4 步的静默预检改成精确的
   「有没有还塞得下的堆」(`HasStackWithRoom`,不动 `modules/bag`);`gem_config_id == 0` 的记录四处统一视同不存在;取下第 6 步按
   「退回的那一条 / 其余」区分日志;`EquipGemSocket` 的不连续计数改成「缺口数 > 越界行数才报」,真正做到同一根因只计一次。

### 10.5 C++ 用例(§7)

1. 「强化未开放 / 下调上限 / 等级中间缺一行」三类场景用 `const_cast` 直接摘 `EquipEnhanceTableManager` 的主键索引(`EditableEnhanceIndex`),
   不是 `Editable` 改行:`MaxEnhanceLevel()` 与取行走的都是主键索引,原地改行上的 id 骗不过它们。`TearDown` 重新 `Load()` 复原。
2. 夹具的金币由 `SetGold` 直接写 `CurrencyComp.values`(`clear_values + add_values`),不走 `CurrencySystem::AddCurrency`
   (那条路要过欠款抵扣、封禁闸与异常获得统计)。扣金币仍走被测代码的正式入口。
3. `PlayerEquipAttributeTest` 的「面板推送发生」靠数一条 WARN 来观察(夹具玩家没有会话组件,`PushPanel` 恰好留下那一条),用到探针的用例
   开头都有一次校准。scene 侧没有「推面板」的可测接缝,这是已知的覆盖方式。它的夹具现在在 `SetUp` 投一段 item 号段(取下要铸号)。
4. 「嵌 → 取」往返后,六项逐位回到原值;当前气血在残血时断言为「不高于原值、且至多少 1 点」(`RescaleCurrent` 两次向下取整)。
   「无净收益」成立,「恰好回到原值」对残血不成立。
5. 没有用例覆盖的契约行为:三张表全空时的校验;下一级缺行 → `kEquipInternalError`;提交段三个带标签的失败分支;实例 `size != 1`;
   脏表同号孔位只下发第一行。前四项从公开接口稳定造不出来。
6. 评审后补强:三处图标 / 名称断言原先两边都是空串(发货表没配图标,夹具物品没有名字),现在先在内存里补值再断言非空;
   新增 `GemRecordWithConfigZeroCountsAsAnEmptySocketAndHealsOnTheNextInlay`;`EnhanceTreatsAnUnpayableGoldCostAsNotEnoughGold` 的注释
   改成只声称它真正验到的对外行为。

### 10.6 robot 冒烟(§7)

1. `relogin` 没有复用一期的:`equipSmokeSession.relogin` 内部把账号写死成 `robot_9103`。新写了 `openEquipForgeSession` / `equipForgeRelogin`
   (约 20 行,只差账号),其余(`call / getBag / gmGrant / throttle / equip / unequip / panelAfter …`)全部复用。
2. 补金币 / 强化石放在每次强化之前(按当时快照里的 `enhance_next` 算缺口),不在预备段;预备段只保证有 1 颗 2101。
3. 多了两条预备断言:人物背包里已有 2103 → 判红(`kEquipGemNotOwned` 靠「从不发放 2103」来造);包里已有 1101 但 `enhance_max_level = 0`
   → 判红且**不再发新装备**(强化未开放时反复重跑不会把这个删不掉东西的账号越跑越脏)。
4. 「背包里强化」恰好把目标推到 +12 的那一轮,「穿上后再强化」改为断言 `kEquipEnhanceMaxLevel` 且两个包不变。
5. 在 §7 列出的断言之外,补了几条由 §3.3 / §4.3 直接推出的断言(成功后 `value == 预览的 enhance_next_value`;面板「恰好」只变物伤 / 法伤;
   三个操作前后目标不换包不挪格;镶嵌后孔位 `attr` 与背包里那颗宝石自报的 `gem_attr` 一致;等等)。对账只比金币这一个数,
   其它币种、欠款、封禁列表不在断言范围。
6. 冒烟镜像的表值集中在 `equip_forge_smoke_scenario.go` 顶部(1101 的伤害表值 40、上限 12、武器两孔类型 1 / 0、2101 / 2201 / 2103 / 2001、
   GM 单次上限 99):策划改这些表必须同步改。

### 10.7 客户端(§6)

1. `NowMs` 是公开可写属性(测试程序集没有 `InternalsVisibleTo`);传 null 恢复默认单调时钟。
2. `ForgeMinIntervalMs` 落地为 **700**(评审指出 500 在默认限流档下持续连点仍会每 4 个包被拒 1 个)。
3. 重拉分类比 §6.1 初稿细:金币 / 材料不足按「服务器给的码是否等于发包时 `CheckEnhance` 的结论」分流;`KEquipEnhanceUnavailable` 同理
   (本地本来就没有预览 → 不重拉,否则每点一次都白拉两份一模一样的快照)。`CheckEnhance` 的检查顺序固定为:未开放 → 满级 → 无预览 → 材料 → 金币。
4. `LastEnhanceOutcome` 的「操作开始」取的是「通过了静默丢弃的闸(在途、发包间隔)之后」:随后被互斥挡下或被本地拒绝的那一次也清空。
5. 取下的本地拒绝只拦「快照里列着这个孔、而且它是空的」;快照里没列出来的孔位号不拦(策划删孔后宝石仍要能按孔位号取下)。
6. 契约没给的几句文案:四句空参数提示;三个新操作回包缺快照时是「服务器未返回最新的背包与装备栏」;本地拒绝的原因复用对应 tip 码的文案。
7. `PlayerBag.cs` 的差异除新增的消息 / 字段外,还有两处文档注释变化(服务器 proto 在基线上已有、客户端上次生成时还没有的改动)。
8. 数据层通过别名引用了 `UI.Ugui.Gameplay` 里的 `EquipBagModel` / `EquipEnhanceCheck`(纯逻辑,同一程序集);以后若拆程序集,这两个类型要跟着数据层走。
9. 没改的:三处既有文案「装备正在更换，请稍候」与界面的「正在更换装备…」对强化 / 镶嵌不贴切(穿脱路径的既有文案,界面文件归 Codex)。

### 10.8 评审与验证记录

- **设计评审**(契约落码前,三个只读视角:服务器可行性 / 资产一致性 / 协议与客户端):30 条发现,采纳的已并入 §2–§9 正文
  (工程登记、发号前置、Tip 追加的两条路径、表校验计数口径、占用与生效两个口径、材料 / 宝石被配成装备的运行期兜底、
  `RemoveItemsExact` 的空表与 0 数量、`ScaleByPermyriad` 的算法与锚点、未初始化实例的拒绝形态、流水 `extra`、读路径静默、
  强化面板缺的字段、孔位显示名、重拉分类、强化结果的发布顺序、限流与发包间隔、客户端落点、逐字格式表、robot 冒烟设计)。
- **代码评审**(落码后,八个只读视角):编译可行性视角对产品代码逐符号核对(对着沙盒导表生成物与主仓头文件)**零发现**;
  其余视角共 14 条,全部是 minor,处置如下 ——
  已修:取下的日志时机、精确的背包满预检、`gem_config_id == 0` 死孔位、`EquipGemSocket` 计数自洽(以上四条见 §10.4 第 9 条);
  三处空转断言与一条注释(§10.5 第 6 条);robot 强化未开放时不再累积装备、对账注释与代码一致(§10.6);
  客户端 `KEquipEnhanceUnavailable` 的无限重拉与发包间隔(§10.7 第 2、3 条);文档里过时的数字与发号现状(本节与 §9)。
  只登记不改:表文件缺失时测试进程 abort(§9.2 的硬前置);`EnhanceTreatsAnUnpayableGoldCost…` 验不到那道显式上限闸(§10.5 第 6 条)。
- **实际跑过的**:补丁脚本八个子命令(真跑三个新的,`--dry-run` 全部八个);沙盒导表一次;robot 的 `gofmt -l -e`;
  客户端离线编译 + NUnit 244 / 244 + 变异测试;若干只读的静态自查脚本(括号配平、用例重名)。
  **没有跑过的**:服务器任何编译、任何 C++ / Go 测试、正式导表、proto-gen、起栈冒烟、Unity。
  (以上是 10-10 的状态;10-13 之后见 §11。)

## 11. 主干验证记录(2026-10-13)

用户 10-13 当面授权 Claude 在**装备这条线**上做「生成 → 并入 main → 串行编译 → 改编译错误 → 跑单测与 robot 冒烟」,
这是 AGENTS §10.1 的一次性例外,范围只到装备线。本节只写**实际跑出来的结果**;没跑的在 §11.6 逐条列出。
证据目录:`E:\work\output\equip-forge-20261013\`(Claude 跑的 C++ / robot 测试日志)、
`E:\work\output\team-follow-20261010\claude-build\run1`、`run2`(C++ 编译,每个工程一份日志 + `results.json`)。

### 11.1 并入与正式生成(Codex 做的,Claude 核对)

- 仓库历史在 10-12 前后被整体改写(提交号全变,另有一次把约 200 个文件的日志字符串英文化)。Codex 把二期的提交重放进 main:
  `6e212616bf`。Claude 把它与原提交逐文件比过:除日志字符串外的行一致,逻辑没有被改动。
- 正式导表(两遍)+ 全量 proto-gen:`0b40715d06`。Claude 在 main 上核对到的结果 ——
  消息号 `SceneBagClientPlayerInlayGem = 250`、`EnhanceEquip = 251`、`RemoveGem = 252`(一期的 246 / 247 / 249 不变);
  tip `kEquipEnhanceUnavailable = 28011` … `kEquipGemBagFull = 28021`,共 11 个,紧跟 `kEquipInternalError = 28010`;
  `equipenhance.json` 12 行、`equipgem.json` 12 行、`equipgemsocket.json` 6 行、`item.json` 61 行;限流表已含三个新方法(10 / 5 / 5)。
- robot 的 vendor 落后于生成物、按默认的 `-mod=vendor` 编不过:`1e3f9e1f68` 只做了 `go mod vendor` 同步。
- 作废的东西:Claude 在隔离工作树里另跑过一次导表与 proto-gen(那一次的号序是 250 = Enhance、251 = Inlay,与 main 不同),
  没有也不会进 main。隔离工作树 `xuanming-server-mmo-equip` 的分支停在旧历史上,**不要合并**。
- 客户端 main `ba60401f` 已合入数据层分支并带上正式的 C# 协议(`MessageIds.cs` 的三个号与上面一致、`EquipErrorTip.cs` 有 11 个新枚举名),
  §6.5 / §9.5 说的 19 处预期 CS0117 已经消失。Claude 在客户端 main `ba60401f` 上做了离线验证(不是 Unity):
  `tools/client_compile_check.ps1` 运行时程序集 429 个文件 0 error;同一套 Roslyn 口径下 Battle 测试程序集 32 个文件 0 error;
  用 Unity 自带的 NUnit 跑装备相关五个用例类 **244 / 244**(`EquipForgeClientTests` 95、`EquipForgeModelTests` 35、`EquipClientTests` 70、
  `EquipBagModelTests` 13、`PlayerFeaturesClientTests` 31)—— 这一次用的是正式生成的消息号与 tip 枚举,不再是占位。
  证据 `E:\work\output\equip-forge-20261013\client-offline\`。

### 11.2 C++ 编译

Debug x64、`VCToolsVersion=14.51.36231`、MSBuild `/m:1 /nr:false`。主仓同一时间只能有一条编译链,所以由「组队跟随」那个会话统一执行
(两条线要的是同一批节点),Claude 只单独重编过 `bag_test.vcxproj`。

| 轮次 | 工程 | 结果 |
|---|---|---|
| run1 | table → proto → core → rpc → grpc_client → modules → battle 库 → scene 库 → gate 节点 | 全部 Exit 0;`modules` / `battle` / `scene` 库在警告即错误下 0 warning |
| run1 | scene 节点 | **链接失败**:21 个 `agones::*` 的 LNK 错误。原因是 `lib\infra.lib` 还是 10-01 的旧产物,编译链里漏了 `infra.vcxproj`;与装备无关 |
| run2 | third_party、muduo、proto_helpers、config、infra、thread_context、session、gate 库、gate / scene / battle 三个节点、`aoi_test` / `bag_test` / `turn_battle_engine_test` / `currency_test` / `cross_zone_test` | 16 个工程全部 Exit 0 |

§9.3 列的「最可能先出错的位置」一条都没有发生:装备的产品代码(`player_equip.cpp`、`equip_attribute_rules.h`、`bag_service.cpp`、
`player_bag_handler.cpp`)与三个大测试文件都是**首次编译即 0 error**,没有为了编过而改任何一行;6 条 `static_assert` 全部成立。

### 11.3 C++ 单测(Claude 跑的,二进制是 run2 的产物)

| exe | 过滤器 | 结果 |
|---|---|---|
| `bag_test.exe` | `PlayerEquipTest.*` | 151 / 151 |
| 同上 | `PlayerEquipAttributeTest.*` | 37 / 37 |
| 同上 | `BagInstanceDataTest.*` | 31 / 31 |
| 同上 | `BagRemoveExactTest.*` | 10 / 10 |
| 同上 | `PlayerFeaturePersistenceTest.EquipInstanceDataRoundTripsThroughDatabaseRecord` | 1 / 1 |
| 同上 | 不带过滤 | 第一次 482 / 488,修了一处测试夹具后 **488 / 488**(见 §11.5) |
| `turn_battle_engine_test.exe` | 不带过滤 | 232 / 232(其中 `Equip*` 56、`TurnBattleEngineTest` 103) |
| `currency_test.exe` | `ClientGmGateTest.*` | 6 / 6 |
| 同上 | 不带过滤 | 144 条里 140 过、3 跳过(要真 Redis / 既有跳过)、**1 失败**:`AssetOpSystemTest.DebitBundleInvalidRecorded`,不属于装备,见 §11.6 |
| `cross_zone_test.exe` | 不带过滤 | 166 / 166(其中 `CrossZoneBagMarshal.*` 6 / 6) |

### 11.4 Go / robot(Claude 跑的)

工作目录 `robot\`,Go 1.26.5,`-mod=vendor`:`go build` 与 `go vet . ./logic/handler` 退出码 0;
`go test -run "TestEquip|TestFeature" . ./logic/handler ./logic/gameobject` 三个包全是 `ok`;`-v` 口径下 253 条 `RUN`、253 条 `PASS`、0 条 `FAIL`
(顶层 `TestEquip*` 32 个,其余是子用例与 `TestFeature*`)。`robot\robot.exe` 是这一批源码编出来的。
Codex 在 `0b40715d06` 上独立跑过同一组,也是 253 通过(见 `docs/PROGRESS.md` 10-13 Codex 那一条)。

### 11.5 顺手修掉的一处测试顺序依赖(`697f5295b1`)

`bag_test.cpp` 里的 `BagSegmentNotReadyTest` 会把物品号段置成「未就绪」,并假定自己最后执行;gtest 按翻译单元排,
`bag_remove_by_guid_test.cpp` 的 7 条用例排在它后面,6 条因为拿不到号段而失败(单独跑是 7 / 7)。
修法只动这一个测试文件:7 个 `TEST` 改 `TEST_F`,夹具 `SetUp` 里重置并投一段物品号段。产品代码没动。
同类的顺序依赖在别的用例组里还有(带 `--gtest_shuffle` 时,种子 7919 / 15838 / 4242 下分别失败 37 / 8 / 7 条,
落在 `BagTest`、`GainBlockServiceTest`、`CompactPolicyTest`、`BagServiceSortTest`、`BagLayoutSwapTest`、`BagPlacementFailureTest`),
**没有处理**;装备相关的用例组在这三个种子下都没有失败。

### 11.6 还没验证的 / 不归本线的

- 三条新日志(`[PlayerEquip] 强化:` / `镶嵌:` / `取下:`)在 main 上仍是中文,而仓库其余日志已在 10-13 统一成英文;不影响功能,
  下次动 `player_equip.cpp` 时顺手改(改了要重编 scene,所以这次没有为它单独动)。
- **客户端**:没有在 Unity 里编过、跑过(EditMode 用例、窗口级用例、实机都没有);证据只有 §11.1 那次离线的 Roslyn + NUnit。
  强化 / 镶嵌的**界面入口没有做**(按分工是 Codex 的文件,接线说明在客户端 `Docs/EquipmentForge-20261009.md`)——
  所以现在进游戏点不到强化与镶嵌,只能靠 robot 冒烟走通这三条协议。
- `AssetOpSystemTest.DebitBundleInvalidRecorded`:期望 `REJECTED` + `kAssetInvalidBundle`,实际 `RETRY` + 27006 ——
  `asset_op_system.cpp` 在校验 bundle 之前先查了 `PlayerBagsComp`。单独跑也失败,属于通用资产通道(帮会 / 聚宝斋)那条线,本线只登记、没有改。
- §8 的已知缺口不变:强化石、宝石、装备本身都没有产出途径(只能 GM 发);洗练、宝石合成、强化保护没做。
- **Java 版(AGENTS §12)**:待做。

### 11.7 起栈冒烟(Claude 跑的,全部通过)

栈由「组队跟随」会话用正式启动器 `tools/scripts/start_game.ps1` 从主仓起(一区;启动器 11:22:42 报 Server started,
日志 `run/logs/game-launcher/20261013-111614-565/launcher.log`):`bin\gate.exe / scene.exe / battle.exe` 是 run2 的产物(源码 `07f9ec3ca9`),
`bin\go_services` 的 11 个 Go 服务与 Java 网关同日重编。它自己的基线先过了:login-test 23 / 23、`TEAM_SMOKE_OK`。
scene 启动日志:`equipment table validation issues=0`;scene 与 battle 的战斗配表指纹相同(`34a72b7b…`)。

在同一套栈上(工作目录 `robot\`,`robot.exe` 是 §11.4 那一份),三次运行退出码都是 0,日志在 `E:\work\output\equip-forge-20261013\smoke\`:

| 运行 | 结果行 |
|---|---|
| `etc/equip_smoke.yaml`(`robot_9103`) | `EQUIP_SMOKE_OK player=2608 steps=9 … affixes=3/3/1 granted=4 physical_attack=50->201 defense=60->72 speed=276->288` |
| `etc/equip_forge_smoke.yaml` 第 1 遍(`robot_9104`,全新账号) | `EQUIP_FORGE_SMOKE_OK player=2615 steps=6 … granted_weapon=true enhance_level=0->1->2 enhance_in_bag=success enhance_worn=success gem=2101 value=30 physical_attack=102->132->102` |
| 同上第 2 遍 | `EQUIP_FORGE_SMOKE_OK … granted_weapon=false enhance_level=2->3->4 enhance_in_bag=success enhance_worn=success gem=2101 value=30 physical_attack=106->136->106` |

与 §9.4 的通过标准逐项对上:第 1 遍 `granted_weapon=true`、`0->1->2`、两次必成;第 2 遍复用同一把(`target` 相同)、`2->3->4`
(第 4 级成功率 9000,这次掷成了);两遍的 `physical_attack` 第一段与第三段相等(取下宝石后回到镶嵌前)。
第 1 遍的分步日志还给出了:7 条拒绝用例全中、强化后基础物攻 40 → 42、穿着时强化面板 100 → 102、嵌入攻击宝石(+30)后 102 → 132、
**重登之后强化等级 2 与宝石仍在**(面板仍是 132)。
scene 侧同时段的 `[PlayerEquip]` 日志与之吻合:4 次强化(扣金币 1000 / 2000 / 3000 / 5000,强化石 1 / 1 / 2 / 2)、2 次镶嵌、2 次取下;
没有 `[ENHANCE_PARTIAL]` / `[INSTANCE_LOST]` / `[GEM_LOST]` / `Table validation` 的 ERROR(同时段仅有的 ERROR 是别的机器人放技能的
`kSkillUnInterruptible`,与装备无关);gate 与 battle 日志 0 条 ERROR。

没有覆盖到的:概率失败那条分支(4 级以后才可能失败,这两遍都掷成了,失败分支只有单测覆盖);真实客户端(界面入口没做,见 §11.6)。
