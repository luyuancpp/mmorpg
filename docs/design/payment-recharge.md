# 真钱充值设计(苹果 IAP / Google Play / 国内安卓 / 小游戏)

> **状态:设计稿第 4 版(2026-10-19),未实现、未编译。** 本稿经过三轮只读对抗评审,共 20 个评审或核实 agent。三轮的发现数:
>
> - 第一轮 47 条;
> - 第二轮新增 15 条,含 4 条 P0;
> - 第三轮新增 15 条,含 2 条 P0。
>
> 每轮核实成立的发现都已改进本稿,修订记录见 §20。**第 4 版的修改没有再经过评审**,R1 动工前建议再复核一轮。R1 起分批落码,每批按 AGENTS §10.1 交 Codex 编译测试。
>
> - **用户拍板(2026-10-19)**:国内、海外两边都要。先做苹果 + 谷歌这套通用骨架,国内渠道作为第二批接上。目前**没有任何开发者账号**,先用本地 `mock` 渠道把整条链路跑通。
> - **分工**(各会话协商,最终由用户拍板):
>   - 共用骨架(支付服务、渠道接口、发货、退款扣回、本文):会话「苹果和安卓应用充值」;
>   - 微信 / 抖音小游戏渠道插件与小游戏登录:会话「微信抖音充值接入」;
>   - 官方物品商品的端到端冒烟:会话「道具交易系统代码」,经同意在 robot 里放一个仅限开发环境的 mock 付款器(§7)。
> - **硬依赖**:本文建在**未合入 main** 的 `origin/snapshot/all-changes-20261018` 的商城发货 / 退款契约之上(§2.2)。

## 0. 一句话结论

**充值 = 买商城里的一个"官方固定价钻石包"。** 订单与钱包各只有一份权威(D-17):订单在 `java/mall_game`,钻石在 C++ `kCurrencyDiamond`。

整个系统靠四条规则保证"不丢单、不重复发、已撤销必扣、撤回必补":

1. **付款事实带修订号。** go/payment 为每笔平台交易维护一个单调递增的修订号:偶数表示已付,奇数表示已撤销。
   - 观测按乐观并发写入:查平台之前先记下基线,提交时修订号没变才算数,所以过时的观测不会盖掉新事实。
   - 只有苹果允许"撤销又撤回",其余渠道的"已撤销"只进不出。
   - 商城按 `(channel, transaction_id)` 的守卫行只接受更大的修订号。
2. **trade 按订单收敛。** 每张充值订单在 `go/trade` 只有一条记录,记着三件事:现在该不该有这笔钻石(entitled)、已经净发了多少(net_granted)、人工核销了多少(written_off)。trade 用"至多一条在途腿"的方式补发或扣回,直到账平,再向商城回执。
3. **扣回可以线性叠加。** 扣回 = 先扣钻石余额,不够的部分记成充值欠款;补发 = 普通入账,先抵充值欠款。把欠款看成负余额,先后顺序不影响结果。
4. **每条路径都有出口。** 孤儿、拒绝、超限、限速暂扣的单子都进人工队列,有定时重评、双人审批的处理入口和终审期限,不会无限期挂着。

`go/payment` 是新增的 Go 全局服务,也是商城唯一的 `payment` 角色调用方(开发环境 robot 冒烟例外,见 §7)。它负责六件事:

- 给客户端发起付款所需的参数;
- 核实付款;
- 维护付款事实;
- 把事实推进到商城;
- 接收退款通知并补漏;
- 执行主动退款、守合规闸。

发钻石、扣钻石只由 `go/trade` 执行。

## 1. 目标与非目标

**目标**

- G1 iOS:App Store 消耗型 IAP 买钻石包,服务端核验 + 退款扣回。
- G2 安卓海外:Google Play 一次性消耗型商品,服务端核验 + 服务端 consume + 退款扣回。
- G3 国内安卓与小游戏:同一套骨架,渠道以插件形式接入(R3)。
- G4 下面任何一种情况都**不丢单、不重复发货、已撤销必扣、撤回必补**,每笔钱都有终局:崩溃、超时、重试、乱序、多副本并发、重复通知、客户端掉线、玩家长期离线。
- G5 本地没有账号也能端到端联调:`mock` 渠道 + robot `recharge-smoke`。

**非目标(v1 不做)**

- 首充双倍、月卡、订阅、礼包组合。v1 只卖"N 元 = M 钻石",每单数量恒为 1。
- 平台游戏币 / 平台钱包,包括微信小游戏游戏币模式(`wx.requestMidasPayment` 或 GameItem 的 `mode="coins"`)和抖音游戏币。它们与 D-17 的单一钱包冲突,**不接**,只做道具直购。
- 聚宝斋人民币寄售收款(以后作为同一骨架的另一种订单用途接入)、玩家提现、卖家结算。
- 客户端 TCP 改 WebSocket(小游戏形态的前置条件)。

## 2. 现状(2026-10-19 核实)

### 2.1 main 上

| 事实 | 证据 |
|---|---|
| 商城订单进程已有幂等下单、库存预占、可信支付回执、取消;付款后只走到 `DELIVERY_PENDING` | `java/README.md`;D-17.4 |
| `POST /internal/payments` 请求体为 `{orderId, channel, transactionId[A-Za-z0-9_-]{1,128}, amountMinor, currency}`。main 上已关闭的订单回 409(**分支已改**,见 §2.2) | `CommerceApi.java`;`CommerceStore.java:117-140` |
| 商城的错误体只有 `{"error": 中文}`,所有冲突都是 409,**没有机器可读的码** | `CommerceController.java:28-30`;`NonceStore.java:51` |
| 商城对 channel **在任何模式下都只做正则校验**,没有渠道白名单。`RUN_MODE` 唯一的作用是禁止 prod 搭配 `REQUIRE_TIDB=false` | 会话「道具交易系统代码」核实 |
| 商城签名格式:`HMAC-SHA256(secret, "v1\nrole\nsubject\nts\nnonce\nMETHOD\n原始URI\nhex(sha256(body))")`,时间窗 ±120 秒,带 nonce。**角色按路径硬编码**,未登记的路径一律算 `gateway`,而 `gateway` 要求 subject 是玩家号 | `HmacAuthentication.java:36-71` |
| SKU 库存是有限 INT。取消订单会无条件释放预占;每个买家最多 5 张未付单;支付窗口 15 分钟;**不自动关单** | `CommerceStore.java:48-49,69-111`;`CommerceProperties.java:14-17` |
| 钻石 `kCurrencyDiamond = 1`;另有金币 0、绑定钻石 2 | `currency.h:8-10` |
| `AddCurrency` 先抵扣欠款(欠款冻结或过期时不抵),所有入账都走它;`DeductCurrency` 不看欠款 | `currency_system.cpp:170-198,223-295`;`player_currency_handler.cpp:50`;`asset_op_system.cpp:788` |
| `AttachDebt` **只记欠款、不扣余额**,还会覆盖原有的 reason / operator / expiresAt;`WaiveDebt` 删掉整条欠款 | `currency_system.cpp:327-414` |
| 运行时欠款表**以币种为唯一键**,加载时后写的覆盖先写的 | `player_currency_comp.h:25,45` |
| 资产通道调用方白名单只有 `guild`、`trade`。`TRADE_CREDIT` 只放行 `TX_AUCTION_BUY`/`TX_TRADE`;`TRADE_DEBIT` 只放行 `TX_AUCTION_SELL`,余额不足时直接按终局拒绝;scene 的 `AssetAbortDebit` 对 5 条流都接受 | `asset_op_auth.cpp:43-52`;`asset_op_system.cpp:89-92` |
| 资产通道遇到玩家离线时:scene 回 NOT_HERE、不记账;Go 侧退避后停放,玩家上线时唤醒重投 | `caller.go:97-110`;`reconcile.go:670` |
| `PlayerInboxAdmin` 是运维面,所有方法共用一把令牌 | `player_inbox_admin.proto:10-51` |
| `PaymentNodeService = 19`、`NODE_PAYMENT = 24` 只有枚举,没有任何使用;`CreateEventSnapshot` 零调用方 | `node.proto:24`;`data_service.proto:438-450` |
| 实名 / 防沉迷 / 年龄:**零实现**。仓库里没有"按区关闭某项功能"的现成能力;全局服务按契约不得按 `cfg.ZoneId` 分支 | 全仓检索;`microservice-zone-contract-20260914.md:17,56` |
| 客户端没有 `com.unity.purchasing`,Unity 版本 6000.6.0f1;网关路由服只转发 gRPC | `Packages/manifest.json`;`microservice-zone-contract-20260914.md:186` |
| `TransactionType` 当前最大值为 29,30–32 在 main 与分支上都空闲(以落地时为准) | `transaction_log.proto` |

### 2.2 未合入的 `origin/snapshot/all-changes-20261018`(本文的硬依赖)

| 内容 | 位置 |
|---|---|
| 已取消订单收到付款时:回 200,state=`REFUND_PENDING`,把回执绑到原订单,写 `commerce_refund_intent`,订单置 5 | `CommerceStore.java:126-136` |
| 发货待办 `deliveries/poll` 和 `/receipt` 用 `delivery` 角色。poll **不出队、不领取**,取前 20 条;receipt 的订单状态更新带 `WHERE status=1`,状态白名单只有 {DELIVERED, REFUND_PENDING, MANUAL};同一订单的第二次回执只要 state 或 proof 不同,就回 409 | `DeliveryStore.java:34-86` |
| `commerce_delivery_intent` 与 `commerce_delivery_receipt` 都以 `order_sn` 为主键 | `schema.sql:53-73` |
| 退款待办 `refunds/poll` 和 `/receipt` 用 `payment` 角色,**目前没有调用方** | `DeliveryStore.java:87-126` |
| 发放物只支持物品:`create()` 要求恰好有 1 条物品发放物,poll 用 INNER JOIN,`/api/skus` 要求 `stock>lock_stock` | `CommerceStore.java:78`;`DeliveryStore.java:27-42` |
| `go/trade` 的 `Mall`:`accept` 用 `RequestKey("official", player, orderId)` 建记录,预读不加锁,同摘要返回旧记录、不同摘要返回 `ErrReplay`;**任意一条出错,整批 Tick 中断** | `mall.go:132-281`;`commerce_repo.go:33-34` |
| Engine:腿按 role 去重,官方 plan 写死 `official_credit`;腿没 applied 就走 `official_refund`;Due = status IN (1,3,4,6,8) ORDER BY updated_ms LIMIT 100,advance 结束时**无条件刷新 updated_ms**;tx_type 按流写死(流 4 = `TX_AUCTION_BUY`) | `engine.go:406-413,448-460,657-766`;`commerce_repo.go:71` |
| 自动放弃只针对 `TRADE_DEBIT`,默认 180 天;放弃行不置 durable | `trade.yaml:173`;`config.go:207` |
| `PlayerAssetSQLCommit` 提交证据;mall_game 没有迁移执行器,`SchemaVerifier` 写死 version==1 | `asset_op.proto`;`SchemaVerifier.java:21-25` |

**待用户确认**:这个分支由谁负责、何时合入 main。

## 3. 总体架构

```
              ┌──────────── 平台(苹果 / 谷歌 / 微信 / 抖音 / 渠道)────────────┐
              │  核验 / 查单 / 通知历史 API              通知(公网 HTTPS 推送)  │
              └──────▲──────────────────────────────────────┬────────────────┘
                     │ 核实、补漏、重评                        │ 到账 / 退款 / 撤回
 ┌────────┐ ②建意图 ┌─┴──────────────────────────────┐       ▼
 │ 客户端 │───────▶│ go/payment(新,全局)            │ ④付款事实同步(带修订号,payment 角色)
 │ Unity  │ ③交收据 │ 渠道插件 · 付款事实库 · 推进任务  │──────────────────────────────▶┌───────────────┐
 │/小游戏 │───────▶│ 补漏 / 重评任务 · 合规闸 · 管理面 │◀─ 只读查单 / SKU、退款待办 ────│ java/mall_game │
 └───┬────┘         └─────────────────────────────────┘                                │ 订单 + 交易守卫 │
     │ ①下单 / 查单 / 取消(经 gate)                                                   └──────┬────────┘
     ▼                                                                                        │ 权益待办(领取式)
 ┌───────────────┐ ⑤ 按订单收敛:差额 > 0 → TRADE_CREDIT / TX_RECHARGE              ┌──────────┐
 │ go/trade Mall  │──────────────────────────────────────────────────────────────▶│ C++ scene │
 │ (已有,扩展)   │          差额 < 0 → TRADE_DEBIT / TX_RECHARGE_REVERSAL         │ 钻石权威  │
 └───────────────┘◀── 收敛后按修订号回执(DELIVERED / VOIDED / MANUAL)──── 商城     └──────────┘
```

| 组件 | 职责 | 不负责 |
|---|---|---|
| `go/payment`(新,`PaymentNodeService=19`,全局) | 客户端支付协议;渠道核验;通知入口、补漏、定时重评;付款事实库;推进任务(同步商城、谷歌 consume);执行商城退款待办;合规闸;管理面 | 订单、发货、扣回、钱包 |
| `java/mall_game`(扩展) | 订单唯一权威;交易守卫;充值 SKU;权益待办(含 HELD 暂扣);按回执推导订单状态;payment 角色只读接口 | 核验平台收据、判断发没发 |
| `go/trade` Mall(扩展) | 下单、查单、取消的门面;**每单收敛**:发、扣、补、核销 | 判断付没付 |
| C++ scene | `AddCurrency`;充值扣回原语、充值欠款、核销原语;资产通道账本幂等 | — |

**支付服务用 Go、不放进 mall_game 的理由**:

- 网关只转发 gRPC;
- `noderegistry`、gate 路由、部署门禁只有 Go 设施;
- D-17.2 第 3 条要求支付回调有独立的可信边界。

苹果没有官方 Go 库。JWS 验签用标准库按 §5.3 的清单自行实现,再用苹果官方 Java 库生成的 golden 向量对拍(§16)。

## 4. 商品与钱包模型

- **钻石包 = 商城官方 SKU,同时满足三个条件:货币发放物、无限库存、每单数量恒为 1。** v1 默认档位如下(用户可改):

  | SKU 标价(CNY 分) | 钻石 | 苹果 / 谷歌商品 ID |
  |---|---|---|
  | 600 | 60 | `diamond_60` |
  | 3000 | 300 | `diamond_300` |
  | 6800 | 680 | `diamond_680` |
  | 12800 | 1280 | `diamond_1280` |
  | 32800 | 3280 | `diamond_3280` |
  | 64800 | 6480 | `diamond_6480` |

- 一个 SKU 的发放物**物品与货币恰好一种**。货币发放物在下单时快照进订单。
- 充值 SKU 的规则:
  - `quantity != 1` 回 400;
  - 不预占、不扣库存,取消时也不释放;
  - 不计入未付单上限;
  - 超过支付窗口后,商城定时关单(状态 4)。**关单只改订单的展示状态,不影响合规额度**(§12)。迟到的付款会复活订单(§10.1)。
- **客户端复用订单以意图状态为准**:只有 go/payment 返回的意图状态为"有效",客户端才复用该订单。收到 `PAY_INTENT_USED`(意图已经有核实过的交易)时重新下单。遇到苹果"家长批准(Ask to Buy)"或谷歌"待付款(PENDING)"时,客户端不主动取消订单。
- **渠道商品映射核对**:`go/payment` 配置 `sku_id ↔ (channel, store_product_id, 期望基准价)`。启动时用 payment 角色的只读 SKU 接口核对四项,任一不一致就拒启:
  - 映射存在;
  - 已上架;
  - 是充值 SKU;
  - **SKU 标价等于期望基准价**。

  拿得到商店价格时,再核对中国店面的价格:谷歌用 Play Developer API 查一次性商品价格;苹果的 App Store Connect API 需要另一把团队密钥,放进部署前检查脚本。不一致时至少要阻断发布。
- 钱包只有 `kCurrencyDiamond`,**不发绑定钻石**。
- 玩家处于货币获取封禁(`BlockCurrency` 或全局 `GainBlock`)时,`CreatePayIntent` 直接拒绝。

## 5. 主流程:苹果 / 谷歌(收据式)

以"玩家 1001 买 diamond_60,6.00 元"为例。

### 5.1 步骤

| 步 | 谁 | 做什么 | 幂等 / 失败处理 |
|---|---|---|---|
| ① | 客户端 → gate → go/trade | `CreateOfficialOrder{sku_id, quantity=1, client_request_id}`,商城建单 `A123` | 商城 `(buyer, request_id)` 唯一 |
| ② | 客户端 → go/payment | `CreatePayIntent{order_id, channel, client_params}`。go/payment 用 order-view 查单,要求:买家等于会话玩家、状态 `AWAITING_PAYMENT`、数量 1、渠道有映射、玩家未被封禁。然后过合规闸(§12)。意图已用 → 回 `PAY_INTENT_USED`;意图已过期 → 在合规行锁下重新预占,再 CAS 回"有效" | 一个订单只有一条意图、一个渠道(§8.1 `order_key`) |
| ② 返回 | | 苹果:`app_account_token = UUID(HMAC(K_bind, "apple:" + order_sn))`。谷歌:`obfuscated_account_id = base64url(HMAC(K_obf, player_id))`,`obfuscated_profile_id = base64url(HMAC(K_bind, "google:" + order_sn))`,两者都是 43 字符。响应带上意图状态 | 不含个人信息 |
| ③ | 客户端 | Unity IAP 拉起付款。苹果先调 `SetAppAccountToken`(状态型 setter),所以**购买流程串行** | |
| ③' | 客户端 → go/payment | 把付款回调里的苹果 `jwsRepresentation`,或谷歌的 `purchaseToken + productId`,交给 `SubmitStoreReceipt`。go/payment 记下会话玩家 `submit_player_id` | MessageLimiter |
| ③核实 | go/payment → 平台 | 见 §5.3 | 平台不可达就回可重试错误,finish=false |
| ③落库 | go/payment | 按 §8.1 的乐观并发规则写 `payment_store_txn`,提交后按 §5.5 决定 finish | |
| ④ | 推进任务 → 商城 | 扫描满足 `resolution=BOUND` 且 `mall_synced_revision < fact_revision` 的记录,按 §8.0 同步。调用前先持久置 `post_attempted=1`。同一意图下的多笔交易,按核实先后顺序同步 | 超时、5xx 退避重试,每次重新生成 nonce。按 §8.0 的应答表对齐本地 |
| ④' | 推进任务 → 谷歌 | 对已绑定的交易调 `purchases.products.consume` | 3 天内必须完成;对方回"已消耗"按成功处理 |
| ⑤⑥ | go/trade → scene | 领取权益待办,按订单收敛(§8.3) | 每单一条记录;至多一条在途腿 |
| 回执 | go/trade → 商城 | 收敛后按修订号回执 | |
| 展示 | 客户端 | 轮询订单,直到出现终态:`DELIVERED`、`REVERSED` 或 `MANUAL` | |

### 5.2 三条到账路径殊途同归

三条路径最终都汇入 `ConfirmStoreTransaction(channel, transaction_id)`,它按 §8.1 的规则观测平台事实,再对齐到本地:

1. 客户端交收据;
2. 平台主动通知(苹果 `ONE_TIME_CHARGE`,谷歌 RTDN);
3. 客户端启动时调 `FetchPurchases()`,把未结束的交易重交一遍。

**通知回包时机**:只在事实落库、事务提交之后,才回 200 或 ack。验签失败或落库失败时回非 2xx。

### 5.3 核实清单

**苹果**:

- 客户端交来的 JWS 只用来取 transactionId 和做预检。唯一的入账依据,是 go/payment 按交易环境调 Get Transaction Info 拿到的 `signedTransactionInfo`。
- 以下字段一律以 GTI 返回为准:`transactionId / productId / appAccountToken / bundleId / environment / quantity / storefront / price / currency / revocationDate / signedDate`。GTI 与客户端 JWS 不一致时,拒绝并告警。
- 本地验签逐条执行:
  1. `alg` 只认 `ES256`;
  2. `x5c` 恰好 3 张;
  3. 信任锚固定为 Apple Root CA - G3,写死指纹;
  4. 叶证书必须带 OID `1.2.840.113635.100.6.11.1`,中间证书必须带 `1.2.840.113635.100.6.2.1`;
  5. 证书有效期用 `signedDate` 校验;
  6. 核对 `bundleId`、`environment`,生产环境再核对 `appAppleId`;
  7. 不得放宽成 `ExtKeyUsageAny` 却不补 OID 检查。
- **价格核对**:`storefront=CHN` 时,要求 `currency=CNY` 且 `price / 10 == SKU 标价(分)`。苹果的 price 以千分之一元为单位。其它店面按配置的价格档位表比对。不符时不自动入账,转人工并告警。

**谷歌**:调 `purchases.productsv2.getproductpurchasev2`,按下表映射为平台事实:

| 条件 | 事实 |
|---|---|
| `purchaseState=PENDING` | Pending,不产生修订号,不当作撤销 |
| `purchaseState=CANCELLED`,或 `productOfferDetails.refundableQuantity < quantity`(本设计 quantity 恒为 1,即 refundableQuantity==0),或本地已有持久化的 voided 证据 | REVOKED |
| 其余 `PURCHASED` | PAID |

- **voided 证据**来自 Voided Purchases API 的确认:收到 `voidedPurchaseNotification` 后,按时间窗查询,确认该 token 在列表里,每日补漏拉到的条目也算。证据写入 `voided_evidence_ms` 后,**任何查询结果都不能推翻它**。注意 RTDN 消息体没有签名,不能直接当证据。
- 其它核对:
  - 商品与意图一致;
  - `obfuscatedExternalProfileId` 能定位到意图;
  - `obfuscatedExternalAccountId` 等于**意图的 player_id** 的派生值。不和会话玩家比,两者不一致时只记日志和指标;
  - `productOfferDetails.quantity == 1`。
- 带 `testPurchaseContext` 的是测试购买(§10.5)。
- 实收金额:用 `orderId` 异步调 Orders API(`orders.get` 或 `batchget`)补写,并与期望价格比对。不符时告警,并**冻结该映射**,停止用它建意图。没有 orderId 的交易保持 `PaidKnown=false`。
- R2 用 License tester 实测"购买后退款"时 `purchaseState` 与 `refundableQuantity` 的实际取值,记进附录 A。

**通用**:

- token 没命中意图,或商品与意图不一致 → 孤儿(§5.4)。
- 数量不为 1 → 进人工。
- 平台事实为 Pending → 只记录,等通知或下次重评。

### 5.4 孤儿、第二笔钱与人工出口

**孤儿**(收据里没有 token,或 token 对不上意图):不自动入账,`resolution=ORPHAN`,进人工队列并告警。人工绑定 `ResolveOrphan` 以 `ORPHAN` 为起点,要求:

- 绑定前实时复核交易未撤销。谷歌孤儿必须在 3 天确认期限内完成绑定,并由服务端 consume。
- 用 `txn_key` 唯一约束防止重复绑定;双人审批,同一事务写审计,审批 id 随请求传给商城留痕。
- **归属证据按渠道区分**:
  - 谷歌:混淆账号号能反查到玩家;
  - 苹果:③' 落库时记下的服务端认证会话玩家 `submit_player_id`。绑定成功后调苹果 Set App Account Token,把订单的 token 写回苹果;
  - 经通知路径进来的苹果孤儿没有会话玩家,只能靠玩家申诉加人工核对。证据不足时,明确引导玩家向苹果申请退款。
- 客户自己提供的收据不能作为唯一的归属依据。
- 绑定后走"无原单入账"(§8.2.4)。

**同一订单收到第二笔已核实交易**:只要 token 派生自该玩家的订单、商品一致,就自动走"无原单入账"的 `EXTRA`(带 `originOrderId`)。

- 原单的第一笔还没同步完成时,商城回 `STATE_CONFLICT`,go/payment 退避重试。
- 超过结构上限时,不拒收,照常建单,权益待办进入 `HELD`(§8.2.4)。

**管理面**(`payment_admin.proto`):

| 方法 | 作用 |
|---|---|
| `ListManual` | 列出 `ORPHAN / OVER_LIMIT / REJECTED_RETRYABLE / REJECTED_FINAL / TEST_DENIED`,以及商城侧的 HELD 待办 |
| `ResolveOrphan` | 绑定孤儿 |
| `RetryManual` | 用当前配置重跑 `ConfirmStoreTransaction`,适用于 `REJECTED_*` 和 `OVER_LIMIT` |
| `ReleaseHeld` | 释放 HELD 待办 |

所有人工动作都要双人审批,并写 `payment_audit`。

**定时重评**:定时扫描 `fact=PAID` 且 `resolution ∈ {ORPHAN, REJECTED_RETRYABLE}` 的交易,按当前配置和平台事实重评。苹果和两段式渠道不会再收到客户端事件,不能只靠事件触发。`OVER_LIMIT` **不因额度重置而自动重评**(§12)。

### 5.5 什么时候让客户端结束交易(finish_transaction)

| 渠道 | 落库后的结果 | finish | 理由 |
|---|---|---|---|
| 苹果 | 任何结果 | true | 苹果不会在服务端重推未 finish 的交易,不 finish 反而会让同一商品买不了 |
| 谷歌 | 已绑定订单 | true | 客户端 `ConfirmPurchase` 触发 consume,服务端 consume 只是补偿 |
| 谷歌 | 孤儿、合规超限、终局拒绝、可重评拒绝 | **false**,服务端也不 consume | 保留"3 天不确认自动退款"的兜底。代价是同一商品 3 天内不能再买 |
| 两段式渠道 | — | 不适用 | |

平台不可达或尚未落库时,一律 false。

## 6. 两段式渠道(小游戏 / 微信支付 / 支付宝 / 国内渠道)

接口按会话「微信抖音充值接入」核实的结果预留,平台事实见附录 A。

- **建意图由服务端签名**:
  - `CreatePayIntent` 带 `client_params`,例如微信 `wx.login` 的 code。插件返回 `channel_params`,客户端原样转交给平台前端 API。
  - 微信小游戏道具直购用 `wx.requestMidasPaymentGameItem`(`signData` 中 `mode="goods"`)。服务端生成整串 `signData`(必须逐字节一致)、`paySig`、`signature`。
  - 抖音安卓用 `tt.requestGamePayment`(`goodType=2`),没有服务端签名。
- **session_key 按需现取**:微信插件用 code 当场调 `jscode2session`,用完即弃,不落库;拿到的 openid 必须与 `payment_channel_identity` 登记的一致。
- **商户单号按尝试派生**:`outTradeNo = 订单派生值 + 尝试序号`,不超过 32 字符,首字符不是下划线。只有"有效"的意图才能生成新尝试。旧尝试的单号经 `payment_intent_ref` 仍能定位到同一订单。
- **到账以平台推送为准**:
  - 每个渠道一条公网回调路由 `/notify/<channel>/<appid>`;
  - `ParseNotification` 可以返回"只回包、不产生事件",用于握手、模拟推送和非支付事件;
  - 按平台格式和时限回包;同一平台单号的重复推送,回和第一次一样的成功;
  - 路由不写死成"只有支付"。
- **查单兜底**:需要用户签名才能查单的渠道,降级为只靠推送和日对账,修订号由事件类型决定(§8.1)。
- **只做道具直购**。

## 7. 渠道插件接口(Go)

```go
// 一个渠道 = 一个实现。骨架只依赖这个接口;渠道私有的密钥、证书都在实现内部,按环境(production/sandbox)各一组。
type Channel interface {
    Name() string // "mock" / "apple" / "google" / "wechat_minigame" / "douyin_minigame" / ...

    // 撤销是否可逆:只有 apple 返回 true(REFUND_REVERSED)。其余渠道的 REVOKED 只进不出。
    RevocationReversible() bool

    // 建意图:收据式渠道只返回绑定用的 token;两段式渠道返回签好名的参数(每次调用是一次新尝试)。
    CreateIntent(ctx context.Context, in IntentInput) (IntentOutput, error)

    // 核实客户端交来的收据。返回值只来自平台核验结果,不得回显客户端字段。
    VerifyReceipt(ctx context.Context, in ReceiptInput) (VerifiedTxn, error)

    // 解析并验签平台通知。验签失败返回 ErrBadSignature:调用方不落任何状态,回非 2xx。
    // events 可以为空(握手、模拟推送、非支付事件)。Ack 在事件落库提交之后才发出。
    ParseNotification(r *http.Request) (events []Event, ack Ack, err error)

    // 查平台当前事实。需要用户签名才能查的渠道返回 ErrQueryUnsupported。
    Query(ctx context.Context, ref TxnRef) (VerifiedTxn, error)

    // 发货确认:谷歌 consume、华为消耗;微信是推送回包失败后的"补通知已发货"(待核实)。不需要的渠道返回 nil。
    Finalize(ctx context.Context, txn VerifiedTxn) error

    // 主动退款。苹果、微信小游戏、抖音小游戏返回 ErrRefundUnsupported;谷歌用 orders.refund(revoke 参数语义待 R2 核实)。
    Refund(ctx context.Context, req RefundRequest) (RefundResult, error)
}

type VerifiedTxn struct {
    Channel        string
    TransactionID  string // 平台交易号原样保存;发给商城前按 §10.4 规范化
    StoreProductID string
    Quantity       int64
    IntentRef      string // 苹果 appAccountToken / 谷歌 obfuscatedProfileId / 两段式渠道的商户单号
    AccountBinding string // 谷歌 obfuscatedAccountId / 小游戏 openid
    Storefront     string
    ListAmount     int64  // 平台给出的标价(苹果 price/10、微信 OrigPrice);拿不到填 0
    PaidAmount     int64  // 平台实收;拿不到填 0,并把 PaidKnown 置为 false
    PaidCurrency   string // 抖音 iOS 为 DIAMOND(§10.2)
    PaidKnown      bool
    Environment    string // production / sandbox
    Fact           Fact   // Paid / Revoked / Pending
    ObservedAtMs   int64  // 平台给出的事实时间(苹果 signedDate);拿不到填 0
    RevokedAtMs    int64
}
```

**`mock` 渠道**:

- 只在 go-zero 顶层 `Mode ∈ {dev, test}` 时允许注册,否则拒启;`MockNotify` 在运行时再核对一次 Mode。
- 收据格式 `mock:<order_sn>:<nonce>`,核实恒成功。
- `MockNotify` 是内部管理 RPC,可以模拟到账、退款、撤回。

**robot 开发环境 mock 付款器(冒烟例外,2026-10-19 与会话「道具交易系统代码」约定)**:

- go/payment 落码前,允许 robot 内置一个仅限开发环境的付款器,只连 loopback、只认 dev 模式的商城,渠道名用 `mock-dev`。
- **现状**:商城在任何模式下都只按正则校验 channel。"生产环境拒收 mock 前缀"要等 V3 的渠道白名单(§8.2.11)落地,在那之前只能靠付款器自身的 loopback + dev 限制。
- 签名逻辑抽成 robot / Go / Java 共用的测试向量。
- go/payment 落码后,robot 改为走 go/payment 的 mock 渠道。

## 8. 数据与改动面

### 8.0 付款事实同步协议(go/payment ↔ 商城)

**修订号**:每笔平台交易 `(channel, transaction_id)` 有一个修订号 `revision`,偶数表示已付,奇数表示已撤销。分配规则见 §8.1。

**go/payment 对商城的调用**。所有调用都带 `revision`、`amountMinor`、`currency`:

| 调用 | 何时用 |
|---|---|
| `POST /internal/payments` | 本地 `resolution=BOUND`(已绑定订单)的交易,**任意修订号**,带 `orderId` |
| `POST /internal/payments/unbound` | 无原单入账(§8.2.4)的交易,**任意修订号**,带 `buyerId, skuId, source, originOrderId / approvalId` |

本地从未绑定、也从未调过商城(`post_attempted=0`)的交易被撤销时,只在本地记录,不调商城。

**商城交易守卫**:`commerce_payment_receipt` 扩展为交易守卫行,主键仍为 `(channel, transaction_id)`,`order_sn` 可空,新增 `revision` 和 `fact` 两列。所有付款路径都先锁这一行:

1. **没有这一行,或请求修订号更大:应用。**
   - **已付**:
     - 守卫已绑定订单:订单处于 `REVERSED` 时,写一条"权益恢复"待办。但只有该订单在撤销前曾向 trade 放行过 entitled=1 的待办,才写恢复待办。被 HELD 暂扣、从未放行的订单,撤回后继续保持 HELD。
     - 守卫未绑定订单:绑定请求里的订单,记回执,写权益待办。
     - 绑定前先查该订单是否已被别的守卫行绑定,是的话回 `DIFFERENT_RECEIPT`,go/payment 改走 EXTRA。
   - **已撤销**:
     - 已绑定订单:订单进入 `REVERSED`,写一条 entitled=0 的待办。
     - 没有这一行:建守卫行并直接绑定请求里的订单(fact=REVOKED)。订单处于 0 或 4 时置为 `REVERSED`,并写一条 entitled=0 的待办,trade 只建墓碑,没有副作用。
2. **请求修订号与当前相等**:幂等,回 200 + 当前状态。
3. **请求修订号更小**:过期请求,回 200 + 当前状态。
4. **奇偶与事实不符**:回 400。唯一键冲突按具体约束映射成 `TXN_USED` 或 `DIFFERENT_RECEIPT`,不得统一映射。

**应答体**:`{code?, revision, fact, orderId, orderState, held}`。go/payment 按下表对齐本地:

| 应答 | go/payment 本地动作 |
|---|---|
| 200,`revision ≥ 本地请求的 revision` | `mall_synced_revision = max(mall_synced_revision, 应答 revision)`;**不得**用商城状态覆盖本地 `fact` |
| 200,`revision < 本地 fact_revision`(理论上不会出现) | 告警,继续推进 |
| `DIFFERENT_RECEIPT` | 按 §5.4 改走 EXTRA |
| `STATE_CONFLICT` | 可重试,退避 |
| `TXN_USED` / `AMOUNT_MISMATCH` | `resolution=REJECTED_FINAL`,记原因,进人工 |
| `NONCE_REPLAY` | 重新生成 nonce,立即重试 |
| 超时 / 5xx / 网络错误 | 退避重试 |

这一协议覆盖了以下情况,全部安全:

- 在途推进与撤销的任意先后;
- 推进超时而商城其实已经提交;
- 推进超时而商城确实没有提交,随后撤销、再撤回:撤回请求带着订单号走 `/payments`,会绑定守卫行并发完整的钻石;
- go/payment 在商城回 200 后、写本地之前崩溃。

### 8.1 go/payment 的库 `mmorpg_payment`

表源是 proto,建表走 `go/schemamigrate`,遵守 D-14 的约束:整数主键(号段发号)、每表至多一个唯一键、键列是整列 `VARCHAR(N) utf8mb4_0900_bin`、应用层先限长。

| 表 | 用途 | 唯一键(最大长度) |
|---|---|---|
| `payment_intent` | 意图:order_sn、player_id、channel、sku_id、合规预占额及所在月键和日键、状态(有效 / 已用 / 已过期 / 已取消)、过期时间(按渠道,见 §12)、当前尝试序号 | `order_key = "order:" + HMAC(K_bind, order_sn)`(≤ 50),与渠道无关 |
| `payment_intent_ref` | 渠道引用(苹果 token、谷歌 profile id、每次尝试的商户单号)→ 意图 id 与尝试序号 | `ref_key = channel + ":" + 引用值`(≤ 100) |
| `payment_store_txn` | 平台交易事实(列见下) | `txn_key = channel + ":" + 规范化交易号`(≤ 161) |
| `payment_notification` | 通知去重与审计 | `notification_key`(≤ 161) |
| `payment_channel_identity` | openid ↔ player,由登录侧写入 | `identity_key`(≤ 191) |
| `payment_spend` | 合规累计(§12) | `spend_key = 身份哈希 + ":" + 周期`(≤ 96) |
| `payment_audit` | 人工动作的双人审批与审计 | 无 |

`payment_store_txn` 的列:

- 交易信息:原始交易号与规范化交易号、商品、数量、标价、实收与币种、环境、测试标记、核验摘要、意图 id、`submit_player_id`;
- 事实:`fact`、`fact_revision`、`last_observed_ms`、`voided_evidence_ms`;
- 推进进度:`mall_synced_revision`(初始 -1)、`post_attempted`、`resolution`(取值 `BOUND / ORPHAN / OVER_LIMIT / REJECTED_RETRYABLE / REJECTED_FINAL / TEST_DENIED`)、`reject_reason`、`finalized`;
- 合规计入:`spend_identity`、`counted_month_key`、`counted_day_key`、`counted_amount`(未计入时为空)。

**观测与对齐规则**(行锁或 CAS 下进行):

1. **乐观并发**:
   - 处理任何事件(客户端收据、通知、补漏、重评、人工动作)时,先读本地的 `(fact, fact_revision, last_observed_ms)` 作为基线,再查平台。
   - 提交事务里 `SELECT … FOR UPDATE`,只有 `fact_revision` 仍等于基线时才应用这次观测;否则丢弃观测,重新查询。最多重查 3 次,超过就延后重试并告警。
   - 第一次 INSERT 撞唯一键时,回到这条路径重查,不能直接拿手里的观测去比较。
   - 苹果的观测带 `signedDate`,不大于 `last_observed_ms` 的一律丢弃。
2. **翻转规则**:观测事实与本地 `fact` 不同时,同一事务里翻转 `fact`,并把 `fact_revision` 加 1。
   - **可逆性按渠道定义**:只有苹果允许 REVOKED → PAID,而且必须是 GTI 实时查询显示 `revocationDate` 已经消失;其余渠道的 REVOKED 只进不出,之后查到"已付"也不翻回。
   - 谷歌另有 voided 证据规则(§5.3)。
   - **不能查单的渠道**(返回 `ErrQueryUnsupported`)修订号由事件类型直接决定:到账 = 0,退款 = 1,不用"和本地不同就加 1"。
3. **推进**:只要 `resolution=BOUND` 且 `mall_synced_revision < fact_revision`,推进任务就按 §8.0 同步。
4. **重评**:`ORPHAN / REJECTED_RETRYABLE` 遇到新事件或定时重评(§5.4)时,按当前事实重评,可以转为 `BOUND`。`OVER_LIMIT` 只能经人工 `RetryManual` 转为 `BOUND`。平台事实变成已撤销时,只更新事实字段。

**这里不存订单状态,也不存余额。**

### 8.2 java/mall_game(V3,依赖 snapshot 分支的 V2)

形态照 V2 先例:`schema.sql`(全新安装)与 `V3__recharge.sql`(升级)同批加表加列,`SchemaVerifier` 的 version 保持 1,核对清单加上新表、新列和新唯一键。理由援引 D-17.1"裁剪 / 扩展后的独立存储合同"。Java 仓 `game-mall` 放到 R4 对齐(§19)。

1. **新表与改表**:
   - 新表 `commerce_sku_currency_grant`、`commerce_order_currency_grant`、`commerce_sku_unlimited`。
   - `commerce_payment_receipt` 扩展为交易守卫:`order_sn` 可空,新增 `revision`、`fact`。守卫行的金额与币种由请求提供。
   - `commerce_delivery_intent` 改为"权益待办":主键 `(order_sn, revision)`;新增 `entitled`、`trade_order_id`;状态取值 `PENDING / ACCEPTED / HELD / SUPERSEDED / QUARANTINE / 终态`。
   - `commerce_delivery_receipt` 加 `revision`,唯一键改为 `(order_sn, revision)`;state 允许 `DELIVERED / VOIDED / MANUAL`,充值 SKU 不接受 `REFUND_PENDING`。
   - 改主键在 TiDB 上的做法:新建表 → 搬数据 → 改名,不在原表上直接改主键。
2. **发放物二选一**:
   - `create()` 要求物品与货币恰好一种;
   - 充值 SKU 强制 `quantity=1`,并跳过预占与未付单上限;
   - `cancel()` 和付款对无限库存 SKU 跳过库存运算;
   - `skus()` 对两种发放物做 LEFT JOIN,对无限库存 SKU 去掉 `stock>lock_stock` 过滤,`available` 取固定上限;
   - `poll()` 对两种快照做 LEFT JOIN,并校验恰好一种;**不在 mapper 里抛错**,不合法的行在独立事务里置 `QUARANTINE` 并告警,其余行照常返回;
   - `DeliveryApi.Intent` 增加 `currencyType / currencyAmount / revision / entitled`;`DeliveryApi.Sku` 和 `OfficialSkuView` 带上发放物类型与无限库存标记。
3. **迟到付款复活**:按 §8.0 绑定时,如果订单为 4 且是充值 SKU,同一事务里把订单重开到 1,写权益待办,不写 refund_intent。物品 SKU 保持分支现有语义。
4. **无原单入账** `POST /internal/payments/unbound`(payment 角色,只用于充值 SKU):一个事务里完成建单、绑定守卫行、写权益待办。`source` 有两种:
   - `EXTRA`:必须带 `originOrderId`,同一事务里核对:原单买家与 SKU 相同、**存在另一条已绑定原单的守卫行**、渠道相同。原单的第一笔还没绑定时,回 `STATE_CONFLICT`。
   - `ORPHAN_RESOLVED`:必须带审批 id,商城落库留痕。

   **暂扣(HELD)**:下面两种情况都**照常建单、照常回 200**,但权益待办状态为 `HELD`,poll 不返回,应答带 `held:true`,并告警:
   - 同一原单的 EXTRA 超过 3 笔;
   - 超过限速阈值(按买家每日、全局每小时两个维度,§14)。

   释放 HELD 用 `POST /internal/payments/release-held`(payment 角色,带审批 id,双人审批并写审计),把待办改为 PENDING。
5. **按回执推导订单状态**,按以下优先级:
   1. 付款事实为已撤销 → `REVERSED`(对客户端是终态);
   2. 存在未释放的 HELD 待办 → `MANUAL`;
   3. 按 trade 对当前修订号的回执推导:`DELIVERED → 2`,`MANUAL → 7`,还没回执 → 1。

   回执规则:
   - 同一 `(order_sn, revision)` 重放是幂等的,比较结局与净额,不比 proof;内容不同回 409;
   - 修订号更大的回执覆盖推导结果;修订号更小的只记录,不改状态;
   - `VOIDED` 只在事实为已撤销时有效;
   - `REVERSED` 加入 view、list、重放集合,以及 trade 的 `mallAnswer` 和客户端的 `CommerceOrderState`(客户端契约,按 AGENTS §12 处理)。
6. **领取式拉取**:
   - `poll()` 只返回 PENDING;
   - trade 在本库事务提交后调 `POST /internal/deliveries/ack`(delivery 角色),以 CAS 把 PENDING 改为 ACCEPTED;
   - 终局回执由 trade 主动上报;`receipt()` 接受 PENDING 或 ACCEPTED 迁移到终态,所以 ack 丢失也没关系;
   - 收到修订号 r 的终局回执时,同一事务里把该订单修订号小于 r、仍为 ACCEPTED 的待办置为 `SUPERSEDED`。
7. **机器可读错误码**:`AMOUNT_MISMATCH / DIFFERENT_RECEIPT / STATE_CONFLICT / TXN_USED / NONCE_REPLAY`。应答体格式见 §8.0。
8. **payment 角色只读接口**:`/internal/payments/order-view`、`/internal/payments/skus`。
9. **退款待办**:增加来源枚举,只自动执行白名单内的来源;增加关闭回执"渠道不支持退款,已转发货";`refunds()` 加游标;充值 SKU 拒收 `REFUND_PENDING`。
10. **路由 → 角色表**:
    - `payments`、`payments/unbound|order-view|skus|release-held` 归 payment;
    - `deliveries/poll|ack|receipt` 归 delivery;
    - Controller 再核一次角色;
    - 补测试:未登记的路径回 404,角色不对回 401。
11. **渠道白名单**:按环境配置;生产环境不收 `mock` 前缀的渠道。
12. **定时关单**:充值订单超过支付窗口后,定时任务把它置为 4。

### 8.3 go/trade(依赖 snapshot 分支):每单收敛

1. **记录**:每张充值订单只有一条记录 `RequestKey("official", player, order_sn)`。workflow 持久保存以下字段:
   - `entitled`、`mall_revision`、`grant_amount A`、`written_off`;
   - 腿计数 `leg_seq`,以及所有腿的结局。

   `net_granted` 不单独存,**在锁下由腿的结局推导**:已 applied 的 credit 腿金额之和,减去已 applied 的 debit 腿金额之和,再加上核销腿的核销额。这样重复推进也不会重复累加。
2. **事件处理**:权益待办就是事件,格式为 `(order_sn, revision, entitled)`。
   - 处理方式:先在事务里 INSERT;撞了唯一键,就在新事务里 `Lock(commerce_id)` 后在锁下处理。**禁止把撞键当成已处理**。
   - `revision <= mall_revision` 的事件只 ack。否则更新 `entitled` 和 `mall_revision`;如果 entitled 从 0 变成 1,把 `written_off` 清零。记录若已 Settled,改回 Delivering。提交后 ack 商城。
   - 没有记录时收到撤销,建一条 `entitled=0` 的记录,也就是墓碑。墓碑不是终态。
   - `accept` 的预读与撞键两条路径都走这套逻辑,不再返回 `ErrReplay`。
3. **收敛**:
   - 目标值 `target = entitled ? A : written_off`,差额 `diff = target − net_granted`。
   - 有在途腿时等待。`diff > 0` 时建 `TRADE_CREDIT` 腿,`tx_type=TX_RECHARGE`;`diff < 0` 时建 `TRADE_DEBIT` 腿,`tx_type=TX_RECHARGE_REVERSAL`。腿的 role 为 `recharge#<leg_seq>`,`leg_seq` 持久递增。
   - `diff == 0` 且没有在途腿:按当前修订号回执(target=A 回 `DELIVERED`,否则回 `VOIDED`),记录置 Settled(Stage="converged"),**移出 Due**。
4. **腿的结局**:
   - credit applied:计入 net_granted;
   - debit applied:同上;debit 不会因余额不足被拒(§8.4);
   - `ABORTED` 或 `ABANDONED`:视为没发生,继续 plan;
   - 带原因码的 `REJECTED`(例如货币封禁):转 Manual,回执 `MANUAL`。
   - Manual 记录收到新修订号时:在同一个锁事务里更新 entitled。如果已经收敛、没有在途腿,就按新修订号回执 `DELIVERED / VOIDED`,并转 Settled;否则对新修订号重发 `MANUAL`。
   - 解封后由人工经收敛入口重排,禁止绕过收敛直接发放。
5. **推进调度**:
   - 有在途腿的记录,只在腿出现结局(终态或 durable)或收到新事件时才推进。trade_commerce 加 `next_check_ms` 列,Due 按它过滤,不再按 updated_ms 轮询停放中的腿。
   - `Engine.Tick` **按记录隔离错误**:某条出错就跳过并计数,另起短事务把它的 `next_check_ms` 按指数退避推后,并告警。workflow 损坏的记录转 Manual 隔离。
   - op id 等 plan 确定要建腿之后再取。
6. **扣回腿永不自动放弃**:放弃候选查询加 `AND tx_type<>TX_RECHARGE_REVERSAL`,`Abandon` 再按 tx_type 拒一次;`ExpiresMs` 和 `DeadlineMs` 都为 0。`TestAbandonOnlyTouchesDebitStream` 补对应用例。
7. **人工核销**:停放中的扣回腿由人工终结,只能二选一,都要双人审批并写审计:
   - 凭证据把腿记为 APPLIED 或 ABORTED;
   - 核销:在锁事务里置 `written_off = net_granted`。

   扣回已应用、已产生充值欠款的情况,核销作为同一记录的一条腿执行:`tx_type=TX_RECHARGE_WAIVE`,由 scene 核销该玩家的充值欠款,回报核销额 X,`written_off += X`。这样撤回时不会重复发放。
8. **可选**:entitled 变为 0 时,如果 credit 腿还在离线停放,把它的 deadline 提前到当前时刻,让下一次投递改发 Abort,从而缩小"先发后扣"的窗口。
9. **tx_type 下沉到腿**:`TradeCommerceLeg` 新增 `tx_type`,`Engine.advance` 优先用它;改 `trade_table.proto` 后重新生成。摘要对新字段兼容。
10. **拉取不中断**:发货和撤销的消费者遇到坏待办时跳过、计数、告警,不中断整批(商城侧的 QUARANTINE 见 §8.2.2)。
11. 首次收敛为 DELIVERED 时,调用一次 `CreateEventSnapshot(EVENT_RECHARGE)`,失败只告警。

### 8.4 proto 与 C++

- `transaction_log.proto` 新增三个类型,号码以落地时为准:
  - `TX_RECHARGE = 30`:入账与补发;
  - `TX_RECHARGE_REVERSAL = 31`:扣回;
  - `TX_RECHARGE_WAIVE = 32`:核销充值欠款。
- `asset_op_system.cpp` 流规则:`TRADE_CREDIT` 放行 `TX_RECHARGE`;`TRADE_DEBIT` 放行 `TX_RECHARGE_REVERSAL` 和 `TX_RECHARGE_WAIVE`。**不新增调用方,也不新增流。**
- **充值扣回原语**:先扣 `d = min(余额, 应扣)`,差额记为充值欠款。这一步永远 APPLIED,`d` 和差额只写进 TransactionLog。
- **核销原语**(`TX_RECHARGE_WAIVE`):核销该玩家的充值欠款,上限为请求额,回报实际核销额。
- **补发就是普通 `TX_RECHARGE` 入账**:`AddCurrency` 先抵充值欠款,运算是线性的。例子:充 6480 后花掉 6000 再退款,扣 480、欠 6000;又充 1000 被抵掉,欠 5000;撤回补发 6480,抵掉 5000,余额加 1480。
- **充值欠款单独成条**:
  - `player_currency_comp.h` 的欠款键改为 `(currency_type, source)`,`currency_comp.proto` 同步改;Load 遇到重复判为损坏,fail-closed;
  - `AddCurrency` 先抵充值欠款,不受 frozen 和 expiresAt 影响,剩余部分再抵 GM 欠款;
  - GM 系列操作(Attach / Waive / Freeze / Adjust / QueryDebts,以及 GmSanction)只作用于 `source=GM`;充值欠款只能用核销原语处理;
  - GM 发放、邮件发放的响应里返回"被欠款抵扣的金额"。
- **副作用**:充值欠款大于 0 时余额必为 0。所有入账都走 `AddCurrency`,没有旁路,所以消费会被自然挡住,不需要另加限制。退款前已经托管出去的钻石(拍卖出价、求购单)不受约束,归 §18.4 的处罚策略处理。
- `proto/payment/payment.proto`:客户端 service `ListPayChannels / CreatePayIntent / SubmitStoreReceipt / GetPayIntent`,响应带意图状态;新增 tip `PAY_INTENT_USED`。
- `proto/payment/payment_admin.proto`:`MockNotify / ListManual / ResolveOrphan / RetryManual / ReleaseHeld / WriteOffReversal`。鉴权用独立的 env 令牌(≥ 32 字节,放在 metadata),NetworkPolicy 只放行运维工具;客户端不收这个文件。
- `proto/payment/payment_table.proto`:§8.1 的表。
- tip 码:`Tip.xlsx` 新开组头 `//payment_error base=<落地时选定> width=1000`,同步 `tip-code-axis.md`、`CMakeLists.txt` 和 `table.vcxproj`。

### 8.5 部署与接入清单(R1 逐项落地,并给出对应的契约测试)

| # | 项 | 契约测试 |
|---|---|---|
| 1 | `proto_gen.yaml` 与 `proto_gen.no-unity.yaml` 加 payment 块(漏了会丢消息号,DV-5) | `tools/scripts/tests/dev_tools_proto_gen_exit.tests.ps1` |
| 2 | `$ProtoGenPostHooks` 加 `payment_postgen.py` | 同上 |
| 3 | D-14:`payment-migrate` Job、`00_init_zone_dbs.sql` 建库授权、连接总账、`go.mod` 的 proto2mysql | `k8s_deploy_contract.tests.ps1` |
| 4 | 号段 biz_tag 四处同步,先跑 `data_service -migrate` | `k8s_deploy_contract.tests.ps1` |
| 5 | 路由表、`Assert-RouterForwardBudget`(Timeout ≤ 4000)、`go_services.ps1`、`go_svc_image.ps1`、go-svc manifest | `go_services_ports.tests.ps1`、`k8s_deploy_contract.tests.ps1` |
| 6 | `start_game.ps1` | `start_game_command_contract.tests.ps1` |
| 7 | `k8s_deploy.ps1` 在 staging / prod 固定 `Mode: pro`;生产环境 `go/trade` 的 `Market.Scope=zone`(§10.5) | `k8s_deploy_contract.tests.ps1` |
| 8 | 通知入口单独端口 + 单独 Ingress,只暴露 `/notify/*`;微信要求 80/443 | `k8s_deploy_contract.tests.ps1` |
| 9 | 发布顺序:基础设施为 `data_service migrate → payment-migrate → go/payment → 路由服 → gate`;商城侧先 trade、后商城 V3 | 发布说明 |
| 10 | **`java/mall_game` 的 K8s 部署**(manifest、Secret、Service)目前没有,是 R1 的前置依赖 | 新增 |

### 8.6 密钥持有

| 进程 | 商城密钥 | 其它 |
|---|---|---|
| go/payment | 只有 `payment` | 苹果 `.p8`、Issuer ID、Key ID;谷歌服务账号;`K_bind`、`K_obf`;各渠道商户密钥(每个环境一组);管理面令牌 |
| go/trade | `gateway` + `delivery` | 资产通道密钥 |

`payment` 密钥能给任意待付订单记"已付",泄露的后果等于"可以免费发钻石"。

- 绑定路径 `/internal/payments` **没有限速**。主要缓解措施是 NetworkPolicy(只允许 go/payment 访问 `/internal/payments*`)、K8s Secret,以及每日三方对账。
- `unbound` 另有结构核对、限速和 HELD 暂扣(§8.2.4)。
- 可选:绑定路径按买家设每日金额告警阈值。

## 9. 退款与扣回

所有影响资金的平台事件都只当作触发信号。go/payment 按 §8.1 观测、对齐,再按 §8.0 同步;trade 按 §8.3 收敛。

| 来源 | 事件 | 补漏 |
|---|---|---|
| 苹果 | `ONE_TIME_CHARGE` / `REFUND` / `REFUND_REVERSED`:按 transactionId 调 GTI,以 `revocationDate` 为准 | **Get Notification History**:每小时按类型分页拉最近一个窗口,用 `notification_key` 去重 |
| 苹果 | `REFUND_DECLINED`:只记录,但会触发 §12 超限单的终审。`CONSUMPTION_REQUEST`:v1 只记录(§18.3) | — |
| 谷歌 | RTDN `voidedPurchaseNotification`:按时间窗查 Voided Purchases API 确认后,写 voided 证据 | Voided Purchases 每日补漏:近 30 天,带 `includeQuantityBasedPartialRefund=true`;按数量部分退款的进人工 |
| 小游戏 | 微信 `minigame_pay_refund_succ_notify` 等。这类渠道不能查单,修订号由事件类型决定 | 日对账 |
| 商城退款待办 | 只处理白名单来源;有退款接口的渠道,调用前再查一次订单 | — |

**对账自动纠偏**:日对账发现"平台已撤销、守卫仍是已付"的差异时,自动对该交易发起一次重查,走同一条处理链,不只是出报表。

**扣回语义**:先扣余额,差额记为充值欠款(§8.4)。例如充 6480 后退款,花掉 6000、剩 480 → 扣 480,记 6000 欠款。

## 10. 边界决定

**10.1 迟到付款**:苹果、微信小游戏、抖音的钱我们退不了,所以充值 SKU 在"订单取消或关单之后才付款"时**一律发货**:商城把订单从 4 复活为 1。

**10.2 价格**

- **商店定价的渠道**:发给商城的金额 = 订单金额。**商城的金额校验对这类渠道是恒等式,起不到保护作用。** 映射是否正确由两处保证:§4 的启动价格核对,§5.3 的逐笔价格核对(苹果用 GTI 的价格;谷歌用 Orders API 补写后比对)。
- **我们定价的渠道**:
  - 微信 `OrigPrice` 必须等于标价;`ActualPrice` 只存,不校验。
  - 抖音 `currency=CNY` 时,`amount_cent` 必须等于标价;`currency=DIAMOND`(iOS 钻石支付)的口径待真机核实,核实前**进人工,不自动拒绝、也不自动发货**。
  - 微信支付 / 支付宝:实收必须等于标价。
  - 不符的拒入账,并告警。

**10.3 一一对应**

- `order_key` 保证一单只有一条意图、一个渠道;`ref_key` 保证一个渠道引用只指向一条意图。
- 同一笔平台交易只入账一次,有两层保证:`txn_key` 与商城守卫主键。
- 订单已被别的交易付过时,走 EXTRA(§5.4)。

**10.4 交易号规范化**

| 渠道 | 商城交易号 | 备注 |
|---|---|---|
| 苹果 | `transactionId` 原样 | |
| 谷歌 | `hex(sha256(purchaseToken))` | 原始 orderId 与 token 另存 |
| 微信小游戏 | `OutTradeNo` | |
| 抖音 | `cp_orderno` | 平台单号另存 |

规则写单测锁死。

**10.5 测试交易与环境**

按苹果的做法,**生产服务器也要能核验沙盒交易**:审核员用的是正式包,付款走沙盒。

- **环境判定**:苹果看 `environment`;谷歌看 `testPurchaseContext`;微信看 `Env`;抖音没有沙箱。
- **只对白名单入账**,判定依据是**意图玩家的 home_zone**:go/payment 入账时调 data_service `GetPlayerHomeZone`,结果必须等于 `ReviewZoneId`;查不到或服务不可用时一律拒绝(fail-closed)。
- 白名单的每一项都有有效期,另有单账号单日上限和累计上限;每笔测试入账都告警,并写审计。
- 测试交易用**独立渠道名**同步给商城(`apple_sandbox`、`google_test`、`wechat_sandbox`),对账时排除在营收之外。
- **审核区的钻石不能流出**(R2 的具体工作项,必须早于提审):
  - 涉及钻石流出的服务:聚宝斋、拍卖、玩家交易(go/trade),帮会捐献,邮件附件,以及其它赠送类功能;
  - 这些服务按发起方和对手方的 home_zone 判断,home_zone 为审核区的玩家与其它区之间的钻石转移一律拒绝;
  - 生产环境要求 `Market.Scope=zone`,并加部署契约断言;
  - 可选:审核区角色禁止跨区匹配和出区传送。
  - **这项工作排不进 R2 时**,如实写明"测试钻石可能流入正式经济,风险由单日上限和累计上限封顶",交用户拍板(§18.6)。
- 白名单之外的测试交易:`TEST_DENIED`,告警。
- 提审时,在审核备注里提供审核区的演示账号。
- RTDN 的 `testNotification` 只回 200。

**10.6 钻石入账失败**:资产通道终局拒绝时,trade 回执 `MANUAL`,**绝不退款**。这是对分支现有行为的修改,见 §8.3.4。

## 11. 安全

- **不信客户端**:"已付"一律由服务端向平台核验,入账字段取平台的核验结果。
- **绑定**:token 由服务端确定性派生,钻石只发给 token 的主人。截获别人的收据抢先提交,拿不到任何东西。
- **密钥**:只经环境变量或 K8s Secret 注入,日志里只写变量名(AGENTS §9)。持有关系见 §8.6。
- **通知入口验签**:
  - 苹果按 §5.3 清单验签,嵌套 JWS 单独验;
  - 谷歌 Pub/Sub push 验 OIDC Bearer,包括签名、`iss`、受众、服务账号邮箱、`email_verified`、`exp`;RTDN 消息体没有签名,只当线索;
  - 微信、抖音按附录 A 的算法验签;
  - 验签失败不落任何状态,回非 2xx。
- **mock、白名单与管理面**:
  - mock 只在 `Mode ∈ {dev, test}` 时可用,商城生产环境也拒收;
  - 管理面用独立令牌 + NetworkPolicy;
  - 人工动作要双人审批、留审计。
- **日志与指标**:玩家 id 只进日志,不进指标 label;交易号只记后 6 位;收据、token、session_key 原文一律不进日志。
- **限流**:客户端协议走 MessageLimiter;`unbound` 的限速见 §8.2.4。

## 12. 合规

- **国内上线收费必须先有版号**(2016 年 44 号文)。小游戏开通虚拟支付也要求版号和实名系统。
- **实名认证**:2021 年 14 号文要求实名注册登录,不能有游客模式。仓库目前零实现。bizID 的申请方式出自华为文档,署方原文未核实。
- **未成年人充值限额**(2019 年通知,现行有效),按**同一企业**旗下所有游戏、所有账号合并计算:

  | 年龄 | 单次上限 | 每月累计上限 |
  |---|---|---|
  | 未满 8 岁 | 不得充值 | — |
  | 8 至 16 岁 | 50 元 | 200 元 |
  | 16 至 18 岁 | 100 元 | 400 元 |

  《未成年人网络保护条例》第 44 条还要求限制单日累计消费,具体数字由用户定(§18.7)。

**闸的实现**(`BuyerEligibility`):

- **主体与范围**:
  - 按实名身份统计(`pi` 或身份哈希),不按游戏账号;
  - 是否适用,由服务端按实名状态和账号注册地区判断;storefront 只用于付款后的核对和告警,不能作为豁免条件;
  - `payment_spend` 有月键和日键,按北京时间计。
- **预占**:
  - `CreatePayIntent` 在 `payment_spend` 行锁下核对"已付 + **全部在途预占** + 本次 ≤ 上限",通过才预占,并把预占所在的月键和日键记到意图上;
  - 同一账号可以有多条在途意图,由这个总量核对兜住;
  - 意图有效期按渠道定:收据式渠道(apple / google)为 `PendingReserveHours`(72 小时),因为服务端无法识别 Ask to Buy 或 PENDING;两段式渠道为支付窗口;
  - **商城关单不释放额度**,额度只在以下三种情况变化:意图过期、玩家主动取消订单(go/payment 在下次建意图时用 order-view 识别)、意图转为"已用";
  - 已过期的意图被复用时,在行锁下重新预占,超限则拒绝。
- **释放与转已付**:
  - 两者在同一个 `payment_spend` 行锁下进行,以意图状态的 CAS 二选一;
  - 转已付时,从原键扣除预占,在到账键计入已付,并写进 `payment_store_txn` 的 `counted_*` 列;
  - **到账周期与预占周期不同时**,在到账周期的行锁下重新核对上限,超限就走下面的超限分支。
- **其它入账路径**(迟到复活、EXTRA、人工绑定)同样计入并核对;超限时 `resolution=OVER_LIMIT`。
- **超限分支**:
  - 有退款接口的渠道:`Channel.Refund`;
  - 谷歌:不确认,让平台 3 天后自动退款;
  - 苹果、微信小游戏、抖音:不发货,通知监护人走平台退款,全程审计,定期复查。
  - **终审期限**:收到 `REFUND_DECLINED`,或超过 30 天仍未退款时,进入人工终审,二选一并写审计:(a) 监护人确认后发货,按发货时刻计入累计,并留合规记录;(b) 维持不发货,由客服跟进平台退款。**不允许无限期停在 OVER_LIMIT**。
  - OVER_LIMIT 只能经人工 `RetryManual` 转为 BOUND,不会因为额度重置而自动重评。
- **退款与撤回**:
  - 撤销时,按 `counted_*` 回退到当初计入的那个周期,最低到 0;
  - 撤回时重新计入并核对;超限就记一条合规事件,进人工。撤回本身仍按 §8.0 同步,商城侧不拒收。
- 生产环境开放国内地区充值、但实名未接入时,拒绝启动。

**平台规则**:

- 苹果:中国区游戏币必须走 IAP。佣金全球标准 30% / 小型企业 15%;**中国区自 2026-03-15 起为 25% / 12%**。
- 谷歌:Billing 库 2026-08-31 起须 ≥ 8。费率:多数市场首 100 万美元 15%;美、欧、英、澳、日已换新结构。
- 微信小游戏:禁止用户间定向转移道具(三-2.5.1),所以微信包要关掉聚宝斋、交易、拍卖;禁止用付费抽奖诱导消费(2.5.2)。

## 13. 客户端(Unity)

- **Unity IAP**:引入 `com.unity.purchasing` 5.4.x(当前 5.4.4),走 v5 流程:`StoreController` → `Connect` → `FetchProducts` / `FetchPurchases` → `OnPurchasePending` 交给服务端 → 按 `finish_transaction`(§5.5)决定是否 `ConfirmPurchase`。
- **iOS**:默认 StoreKit 2,从 `jwsRepresentation` 取 JWS。`SetAppAccountToken(Guid)` 是状态型 setter,每次购买前设一次,购买串行化。真机要核实 JWS 里确实带上了 token。
- **安卓**:设混淆号;**不开启多数量购买**。实际消耗时点是客户端的 `ConfirmPurchase`,服务端 consume 只是补偿(R2 真机核实)。
- 每次启动时,调 `FetchPurchases()` 把未结束的交易重交一遍。
- **订单复用**:以 go/payment 返回的意图状态为准;收到 `PAY_INTENT_USED` 就重新下单。
- **UI**:
  - 钻石包列表、付款遮罩、到账提示;
  - 订单为 `REVERSED` 时提示"已退款",为 `MANUAL` 时提示"处理中,请联系客服";
  - 国内包在实名接入前隐藏充值入口;
  - 微信在 `wx.checkIsSupportMidasPayment` 不支持时,连商城都不显示。
- **国内安卓包不用 Unity IAP**(R3)。

## 14. 配置与开关

```yaml
Mode: pro                     # go-zero 顶层;dev/test 才允许 mock;K8s staging/prod 由部署脚本强制 pro
Payment:
  Enabled: false              # 总闸,默认关
  Channels:
    mock:   { Enabled: false }
    apple:
      Enabled: false
      BundleId: ""
      AppAppleId: 0
      AllowedEnvironments: [Production, Sandbox]
      Production: { IssuerIdEnv: PAYMENT_APPLE_ISSUER_ID, KeyIdEnv: PAYMENT_APPLE_KEY_ID, PrivateKeyEnv: PAYMENT_APPLE_P8 }
      Sandbox:    { IssuerIdEnv: PAYMENT_APPLE_ISSUER_ID, KeyIdEnv: PAYMENT_APPLE_KEY_ID, PrivateKeyEnv: PAYMENT_APPLE_P8 }
      PriceTiers: {}          # 店面 → SKU → 期望价,用于逐笔价格核对
    google: { Enabled: false, PackageName: "", ServiceAccountEnv: PAYMENT_GOOGLE_SA_JSON, PubSubAudience: "", PubSubServiceAccount: "" }
    # wechat_minigame / douyin_minigame:由「微信抖音充值接入」会话补,production / sandbox 两组密钥变量名
  TestAccounts: { ListEnv: PAYMENT_TEST_ACCOUNTS, ReviewZoneId: 0, DailyCapMinor: 0, TotalCapMinor: 0 }
  Skus:
    - { SkuId: 1001, ExpectedPriceMinor: 600, Apple: diamond_60, Google: diamond_60 }
  Mall: { BaseUrl: "http://127.0.0.1:18120", PaymentSubject: payment-adapter, PaymentSecretEnv: COMMERCE_PAYMENT_SECRET }
  Binding: { BindKeyEnv: PAYMENT_BIND_KEY, ObfKeyEnv: PAYMENT_OBF_KEY }
  AdminTokenEnv: PAYMENT_ADMIN_TOKEN
  Rescan: { OrphanEveryMinutes: 60, ReconcileDailyAt: "04:00" }
  Compliance: { RealNameProvider: none, DailyLimitMinor: 0, PendingReserveHours: 72, OverLimitFinalReviewDays: 30 }
# 商城侧(java/mall_game):
#   commerce.channels-allowed: 按环境配置的渠道白名单(生产不含 mock*)
#   commerce.unbound.per-buyer-daily: 3        # 超过即 HELD
#   commerce.unbound.global-hourly: 200        # 超过即 HELD
#   commerce.unbound.extra-per-origin: 3       # 超过即 HELD
```

## 15. 可观测性、告警与对账

**指标**(label 只用 channel / result):

- 核验数量与耗时;
- 通知数,按类型和验签结果;
- 推进结果;
- 观测冲突重查次数;
- 孤儿、人工、HELD 队列长度;
- 谷歌未 consume 的最长时长;
- 苹果通知最后成功时间;
- 补漏新事件数;
- trade 未收敛订单数、扣回腿最长停放时长、Engine 单条推进连续失败次数。

**告警**:

1. 已核实超过 10 分钟,商城仍未确认;
2. 当前修订号的权益待办领取后超过 30 分钟仍未收敛(玩家在线时);
3. 谷歌超过 48 小时未 consume;
4. 验签失败率突增;
5. 人工队列(含 ORPHAN / OVER_LIMIT / REJECTED_* / HELD)非空;
6. 出现测试交易;
7. 补漏拉到新事件;
8. 超过 N 小时没有苹果通知;
9. `unbound` 达到阈值;
10. 扣回腿停放超过 30 天;
11. Engine 单条推进连续失败;
12. 观测冲突重查超过上限。

**对账**:

- 每日对比"平台交易 ↔ 商城守卫 ↔ trade 收敛记录"三方差异。测试渠道和核销额单列;"平台已撤销、守卫已付"这类差异自动触发重查。
- 苹果以 Notification History 为权威补漏来源;谷歌以 Voided Purchases 和 Orders API 为准。
- 每月对平台财务报表。

## 16. 测试与验收

**单测**:

- **苹果验签**:用官方 Java 库生成 golden 向量。反例覆盖:内容被篡改、bundleId 不对、环境不对、证书链断裂、缺 OID、`x5c` 不是 3 张、`alg` 不是 ES256、嵌套 JWS 被篡改、客户端 JWS 与 GTI 不一致、中国店面价格不符。
- **谷歌事实映射**:PURCHASED 且 refundableQuantity=0 → REVOKED;PENDING → Pending;有 voided 证据而查询返回 PURCHASED → 仍为 REVOKED;生产环境非白名单测试购买 → 不入账;`quantity≠1` → 人工;会话玩家与意图玩家不一致 → 照常入账给意图玩家。
- **观测并发**:基线修订号变化时丢弃观测并重查;苹果 `signedDate` 更旧的观测被丢弃;非苹果渠道 REVOKED 不翻回;不能查单的渠道,修订号按事件类型决定。
- **合规**:
  - 8 / 16 / 18 岁边界,单次、日、月上限;
  - 全部在途预占的总量核对;
  - 跨月、跨日到账时重新核对;
  - 过期与到账同时发生;
  - 意图过期后复用;
  - 撤销后按 `counted_*` 回退;撤回后重新计入并核对;
  - 超限单的终审路径。
- **商城守卫**:修订号更大则应用、相等则幂等、更小则回当前状态、奇偶不符则拒绝;撤销时没有守卫行但带订单 → 绑定并置 REVERSED;订单已被别的守卫绑定 → `DIFFERENT_RECEIPT`。
- **trade 收敛**:
  - diff 正、负、零;
  - 腿 role 唯一;
  - ABORTED 视为没发生;
  - Manual 记录收到新修订号时的处理;
  - 核销(written_off)后不再生成扣回;核销后撤回不额外发放;再次退款照常扣回。
- **充值欠款**:与 GM 欠款共存,重载后两条都在;`AddCurrency` 先抵充值欠款;`WaiveDebt` 不触及充值欠款;核销原语;§8.4 的线性例子。
- **商城回执**:同一 `(order_sn, revision)` 重放幂等;修订号更大的回执覆盖;SUPERSEDED;QUARANTINE 坏行不影响同批其它行。

**集成**(真实 TiDB,mock 渠道):

- **并发与崩溃**:
  - 并发重交同一收据;通知与收据同时到;
  - **两副本并发,旧观测后提交**(注入 GTI 延迟)→ 不发生错误翻转;
  - 落库后、调商城前崩溃;商城 200 后崩溃。
- **推进与撤销的时序**:
  - **推进超时但商城已提交 → 撤销 → 最终扣回**;
  - **推进超时且商城未提交 → 撤销 → 撤回 → 最终发满 A**;
  - 同样的前提下,玩家用同一订单再付 T2 → 撤回 T → T2 走 EXTRA 成功;
  - 撤销与在途推进并发;
  - EXTRA 或人工绑定推进超时后撤销;
  - 订单处于 MANUAL 时撤销;
  - **微信退款推送先于发货推送的重试到达 → 不发货,订单 REVERSED**。
- **订单状态**:
  - 已取消或已关单后付款 → 复活;
  - Ask to Buy / 谷歌 PENDING:订单被定时关单后到账 → 复活发货,额度在 72 小时内保留;
  - 第二笔钱 → EXTRA;第 4 笔 EXTRA → HELD;
  - unbound 超限 → HELD → 释放后发货;HELD 单经历退款、撤回 → 仍为 HELD,不发货;
  - 攒满 5 张未付充值单仍可下单。
- **trade 收敛**:
  - 撤销落在 accept 与回执之间;
  - **撤销落在玩家离线停放期间 → 最终净额为 0**;
  - 墓碑后撤回 → 发满 A;
  - 扣回挂起时撤回 → 互相抵消;
  - 腿被拒后撤回再解封;
  - 退款 → 撤回 → 再退款;
  - 撤销 → DELIVERED → 撤回 → DELIVERED;撤销 → MANUAL → 撤回 → MANUAL;
  - 扣回时余额足够、部分不足、为 0;
  - 扣回腿离线超过放弃门槛后上线,照常扣回。
- **规模与隔离**:
  - **1000 条离线停放的充值记录 + 1 条每次都出错的坏记录**:新充值和拍卖仍在 SLO 内推进;已收敛的记录不在 Due 里;
  - 21 条离线待办不堵塞其它玩家;
  - 坏待办不中断批次;
  - 充值腿被拒 → MANUAL,不退款。
- **审核区**:审核区账号的钻石转往非审核区一律被拒(R2)。

**robot `recharge-smoke`**:登录 → 列 SKU → 下单 → mock 付款 → 到账 +60 → mock 退款 → 扣回 → mock 撤回 → 补回。

**沙盒**(R2):苹果沙盒账号 + TestFlight;谷歌 License testers。实测谷歌退款后 `purchaseState` 与 `refundableQuantity` 的值。抖音没有沙箱。

## 17. 分期

| 期 | 内容 | 需要用户做的 | 依赖 |
|---|---|---|---|
| R0 | 本文 + 各会话对齐 + 三轮对抗评审;**动工前再复核一轮第 4 版** | 拍板 §18 | — |
| R1 | go/payment 骨架 + mock 渠道 + 商城 V3(交易守卫、权益待办、领取式、HELD、充值 SKU)+ trade 每单收敛与调度隔离 + C++ 扣回、核销、充值欠款 + `TX_RECHARGE*` + 接入清单 + robot `recharge-smoke` | **批准 30+ 文件的改动范围与分批计划**(AGENTS §10.2)。每批边界:proto 源与全部重生成物同批,每批能独立编译,交 Codex 编译测试 | snapshot 分支合入 main;mall_game 的 K8s 部署 |
| R2 | 苹果 / 谷歌真实核验、通知、补漏、价格核对 + 客户端 Unity IAP 与充值界面 + **审核区钻石不出区**(§10.5) | 苹果、谷歌开发者账号与商品配置(附录 B) | R1 |
| R3 | 国内:实名 + 防沉迷 + 聚合 SDK 或逐家接入 + 小游戏插件(另一个会话) | 版号、bizID、各渠道账号 | R1 |
| R4 | Java 版服务端对齐(game-mall V3 等) | — | R1 |

## 18. 待决问题

1. **snapshot 分支归属与合入时间**(阻塞 R1)。
2. 钻石包档位(§4)是否照用。
3. 苹果 `CONSUMPTION_REQUEST` 是否回传(需征得用户同意分享数据、写进隐私政策,12 小时内回传)。
4. 退款扣回之外是否附加处罚,包括对退款前已托管出去的钻石(拍卖出价、求购单)怎么处理。
5. Java 版排期,以及 R1 客户端契约与 Java 同批的做法(§19)。
6. 审核区与测试白名单:谁来加、上限多少;审核区钻石不出区是否在 R2 做完(否则接受"风险由上限封顶");是否接受"审核员自己注册的新号买不了"。
7. 未成年人单日消费上限。
8. 限速阈值(§14 商城侧的三项默认值)是否合适。

## 19. 双版本(AGENTS §12)

两版共享的是客户端契约:`payment.proto` 的客户端 service、`payment_error` tip 段(含 `PAY_INTENT_USED`)、`CommerceOrderState` 新增的 `REVERSED`。

- 这些契约按规则应与 Java 版同批落地,Java 侧至少要有协议加一个"暂未开放"的实现。
- 做不到同批时,在 `PARITY.md` 和 `PROGRESS.md` 登记"待做 + 原因",交用户确认。
- Java 侧的服务端实现(苹果有官方 Java 库,谷歌有官方 `google-api-services-androidpublisher`)以及 game-mall V3 对齐,放到 R4。

## 20. 修订记录

- **v1**(2026-10-19):初稿。
- **v2**(2026-10-19):按第一轮评审(47 条)与「微信抖音充值接入」会话的意见修订:
  - 迟到付款按 snapshot 语义处理;
  - 扣回改为 scene 原语;
  - 撤销裁决移到 trade;
  - tx_type 下沉;
  - 商城改动面列全;
  - 苹果验签清单与补漏;
  - 测试交易与合规改写;
  - 小游戏接口名更正。
- **v3**(2026-10-19):按第二轮复核(15 条新发现,含 4 条 P0)重写核心模型:
  - 付款事实修订号 + 商城交易守卫;
  - trade 每单收敛;
  - 补发线性化;
  - 领取式拉取;
  - 扣回腿永不放弃;
  - 按回执推导订单状态;
  - 欠款按来源分条;
  - 谷歌按意图玩家比对,孤儿不确认;
  - REVOKED 对齐平台事实;
  - 合规预占规则;
  - 测试白名单加上限;
  - unbound 增加结构核对;
  - 商户单号按尝试派生;
  - 登记 robot mock 付款器例外。
- **v4**(2026-10-19):按第三轮复核(15 条新发现,含 2 条 P0;第二轮的 22 项里 12 项部分改到位)修订:
  - **观测乐观并发**(基线修订号 + 苹果 signedDate),撤销可逆性按渠道定义,不能查单的渠道修订号由事件类型决定,商城应答只推进 `mall_synced_revision`(V3-01,P0);
  - **每条路径都有出口**:苹果孤儿以会话玩家为证据并回写 token,定时重评,完整管理面,超限终审期限(PAY3-01,P0);
  - 谷歌事实映射表与 voided 证据(V3-02);
  - 已绑定的交易任何修订号都走 `/payments` 并带订单号;撤销时若无守卫行则绑定并置为 REVERSED;删掉 `/fact`;去掉 UNKNOWN_TXN;给出应答体与逐码对齐表(V3-03、R7);
  - trade 已收敛记录移出 Due,改用 `next_check_ms` 调度,按记录隔离错误(V3-04);
  - 腿 role 序号化,net_granted 由腿的结局推导,ABORTED 视为没发生,Manual 记录处理新修订号,HELD 单撤回后不放行(V3-05);
  - 回执与待办按 `(order_sn, revision)`,新增 SUPERSEDED / QUARANTINE(V3-06、PAY3-08);
  - 人工核销 `written_off` 与 `TX_RECHARGE_WAIVE`(V3-07);
  - HELD 暂扣与 release-held,删掉 RATE_LIMITED_MANUAL,如实写明绑定路径没有限速(PAY3-02);
  - `PAY_INTENT_USED`、以意图状态复用订单、EXTRA 超限进 HELD、`STATE_CONFLICT` 可重试(PAY3-03);
  - 意图有效期按渠道设定,关单不释放额度,改为全部在途预占的总量核对(PAY3-04);
  - 启动和逐笔价格核对,删掉"商城金额校验防映射错误"的错误说法(PAY3-05);
  - 白名单按 home_zone 判定,审核区钻石不出区列为 R2 工作项(PAY3-06);
  - 跨周期重新核对、`counted_*` 列,OVER_LIMIT 只能人工转 BOUND(PAY3-07);
  - 契约测试逐项点名;补 MANUAL 撤销与 Ask to Buy 用例,修正与定时关单的矛盾。

## 附录 A 平台事实(2026-10-19 核实,带官方链接)

**苹果**

- **核验**:`GET https://api.storekit.apple.com/inApps/v1/transactions/{id}`;沙盒用 `api.storekit-sandbox.apple.com`;2026-05-05 起推荐这两个域名。[Get Transaction Info](https://developer.apple.com/documentation/appstoreserverapi/get-transaction-info)
- **鉴权**:ES256 JWT。`kid` 填 Key ID,`iss` 填 Issuer ID,`aud` 填 `appstoreconnect-v1`,`bid` 填 Bundle ID;有效期 ≤ 60 分钟;用 `.p8` 签名。[生成 JWT](https://developer.apple.com/documentation/appstoreserverapi/generating-json-web-tokens-for-api-requests)
- **验签**:规格对齐官方库的 `AppleExtensionCertPathChecker`(两个 OID)和 `ChainVerifier`(链长 3)。官方库只有 Swift / Java / Python / Node,没有 Go。`verifyReceipt` 已废弃。[Java 库](https://github.com/apple/app-store-server-library-java)
- **appAccountToken**:必须是 UUID。2025-06 起,服务端可用 Set App Account Token 补设或改写(用于孤儿绑定后回写)。[文档](https://developer.apple.com/documentation/appstoreserverapi/appaccounttoken)
- **通知 V2**:`ONE_TIME_CHARGE`、`REFUND`、`REFUND_DECLINED`、`REFUND_REVERSED`、`CONSUMPTION_REQUEST`(须 12 小时内回传)。生产环境失败重试 5 次(1/12/24/48/72 小时),沙盒只发一次;漏掉的用 Get Notification History 补。[类型](https://developer.apple.com/documentation/appstoreservernotifications/notificationtype) [重试与补漏](https://developer.apple.com/documentation/appstoreservernotifications/responding-to-app-store-server-notifications)
- **审核**:审核员用正式包、沙盒付款,生产服务器需要能核验沙盒交易。[Validating receipts](https://developer.apple.com/documentation/storekit/validating-receipts-with-the-app-store)
- **中国区佣金**:2026-03-15 起为 25% / 12%。[公告](https://developer.apple.com/news/?id=dadukodv)

**谷歌**

- **Billing 库**:最新 9.1.0;2026-08-31 起必须 ≥ 8。[发布记录](https://developer.android.com/google/play/billing/release-notes)
- **productsv2**:不返回价格。字段有 `purchaseState`(PURCHASED / CANCELLED / PENDING)、`testPurchaseContext`(只对测试购买设置)、`productOfferDetails.quantity / refundableQuantity`(后者反映部分退款和全额退款)、`obfuscatedExternalAccountId/ProfileId`。**文档没有承诺退款会把 purchaseState 改成 CANCELLED**,所以本设计同时看 `refundableQuantity` 和 voided 证据。实收用 Orders API;退款用 `orders.refund`。[productsv2](https://developers.google.com/android-publisher/api-ref/rest/v3/purchases.productsv2)
- **consume**:`purchases.products.consume` 同时算确认;3 天内不确认会自动退款。[一次性商品](https://developer.android.com/google/play/billing/lifecycle/one-time)
- **服务账号**:启用 API、建服务账号、在 Play Console 邀请并授权("查看财务数据""管理订单和订阅");已不需要关联 Cloud 项目。[文档](https://developers.google.com/android-publisher/getting_started)
- **RTDN 与 Voided Purchases**:RTDN 要授权 `google-play-developer-notifications@system.gserviceaccount.com`,并选"所有订阅和一次性商品事件"。Voided Purchases 覆盖近 30 天,每天 6000 次、每 30 秒 30 次。[RTDN](https://developer.android.com/google/play/billing/rtdn-reference) [Voided](https://developers.google.com/android-publisher/voided-purchases)
- **费用**:开户 25 美元;多数市场首 100 万美元 15%。[费率](https://support.google.com/googleplay/android-developer/answer/112622)

**Unity IAP**:5.4.4(2026-10-02),iOS 用 StoreKit 2,Android 用 Billing 9.0.0;有 `IAppleStoreExtendedService.SetAppAccountToken(Guid)`;只支持 App Store 和 Google Play。[API](https://docs.unity3d.com/Packages/com.unity.purchasing@5.4/api/UnityEngine.Purchasing.IAppleStoreExtendedService.html)

**微信小游戏**(会话「微信抖音充值接入」核实)

- **道具直购**:前端 `wx.requestMidasPaymentGameItem`,服务端接口 `/wxa/game/*`。
  - `signData` 是 JSON 串,字段有 `mode="goods"`、`offerId`、`buyQuantity`、`env`、`currencyType=CNY`、`platform=android`、`zoneId`、`productId`、`goodsPrice`、`outTradeNo`、`attach`。
  - `outTradeNo` ≤ 32 字符,字符集 `[0-9A-Za-z_-|*@]`,不能以下划线开头,只能用一次。
- **签名**:
  - `paySig = hex(HMAC-SHA256(AppKey[env], "requestMidasPaymentGameItem&" + signData))`;
  - `signature = hex(HMAC-SHA256(session_key, signData))`;
  - AppKey 分 0 正式 / 1 沙箱两把;iOS 不支持沙箱。
- **到账推送**:
  - 事件名 `minigame_game_pay_goods_deliver_notify`,`pay_event_sig = hex(HMAC-SHA256(AppKey[env], Event + "&" + Payload))`;
  - Payload 含 `OpenId / Env / OutTradeNo / GoodsInfo{ProductId, Quantity, ZoneId, OrigPrice, ActualPrice, Attach, OrderSource} / PayChannel`;
  - 回包明文 `{"ErrCode":0,"ErrMsg":"Success"}`;3 秒超时;只能用 80/443 端口;推 15 次,约 24 小时;`IsMock=true` 时回成功,但不产生事件。
- **退款**:`minigame_pay_refund_succ_notify`,12 小时内推 13 次;没有主动退款接口。
- **查单**:`pay_v2.queryOrder`,uri 未核实,可能需要用户签名。
- **iOS**:2026-01 起经苹果 Mini Apps Partner Program 开放;要求 iOS 15+、基础库 3.10.3+、微信 8.0.68+;先调 `wx.checkIsSupportMidasPayment`。
- **分成与开通**:iOS 70%(2026-12-31 前限时约 88%)、安卓 60%。开通要求已认证主体、实名系统、版号和软著自查报告。

**抖音小游戏**(同一会话核实)

- **安卓**:`tt.requestGamePayment{mode:"game", env:0, currencyType:"CNY", platform:"android", goodType:2, orderAmount, goodName, customId, extraInfo}`。要求基础库 3.47.0+、抖音 32.0.0+。没有服务端签名。
- **iOS**:`tt.openAwemeCustomerService`,`currencyType:"DIAMOND"`,1 元 = 10 钻,iOS 用户多付约 30%。`amount_cent` 的口径待核实。
- **回调**:
  - 验签:`token/timestamp/nonce/msg` 按字典序排序后拼接,做 SHA1;配置时的 GET 要回 `echostr`;
  - `msg` 含 `appid / cp_orderno / cp_extra / order_no_channel / amount_cent / amount_coin / currency`;
  - 回 HTTP 200,否则重试 16 次,约 4 小时 45 分;
  - 官方 Go 示例的 `make([]string,4)` 有坑,别照抄。
- **查单**:`queryPayState?orderno=<customId>`,返回 `unsuccess` 不代表终局。
- **其它**:没有沙箱;没有主动退款接口;必须有版号,并在 `wlc.nppa.gov.cn` 添加抖音渠道;开发者一般拿 60%。

**国内其它**

- **华为 IAP**:`POST {rootUrl}/applications/purchases/tokens/verify`,`Authorization: Basic base64("APPAT:"+token)`,用 RSA 公钥验签;中国站 rootUrl 未核实。
- **微信支付 APIv3**:APP 下单用 `/v3/pay/transactions/app`;回调验签串是"时间戳\n随机串\n报文\n",用 AEAD_AES_256_GCM 解密;5 秒内回包,重试 15 次约 24 小时;注意 `WECHATPAY/SIGNTEST/` 开头的探测包。
- **支付宝**:用 `rsaCheckV1` 验签;回包规则未核实。
- **聚合 SDK**:QuickSDK 仍在更新;渠道分成常见约 5:5,未核实。
- **法规**:[版号](https://www.nppa.gov.cn/xxfb/zcfg/gfxwj/201606/t20160602_4492.html)、[实名](https://www.gov.cn/zhengce/zhengceku/2021-09/01/content_5634661.htm)、[充值限额](https://www.gov.cn/zhengce/2019-11/05/content_5448848.htm)。2023-12 与 2026-09-18 的两份征求意见稿都未生效。

## 附录 B 用户需要亲自办的事(Claude 不能代办)

1. **苹果**:
   - 加入 Apple Developer Program(国内 688 元/年);
   - 签 Paid Applications Agreement,填银行和税务信息;
   - 建 App 和 6 个消耗型商品;
   - 生成 App Store Server API 的 `.p8`,记下 Issuer ID 和 Key ID;
   - 建沙盒账号;
   - 配置通知 V2 的生产和沙盒地址;
   - 提审时提供审核区的演示账号。
2. **谷歌**:
   - 注册 Play Console(25 美元);
   - 建 App 和 6 个一次性商品,**不要开启多数量购买**;
   - 建服务账号并授权;
   - 配置 Pub/Sub;
   - 登记 License testers。
3. **国内(R3)**:版号、实名 bizID、渠道账号、小游戏虚拟支付开通、商户号。
4. 密钥**不要发到聊天里**。直接写进本机环境变量或 K8s Secret,告诉 Claude 变量名即可。
