# 真钱充值设计(苹果 IAP / Google Play / 国内安卓 / 小游戏)

> **状态:设计稿第 2 版(2026-10-19),未实现、未编译。** 第 1 版经过五个角度的只读对抗评审(代码事实、资金正确性、安全、平台规则、仓库规范;10 个评审 agent,47 条发现,其中 2 条被驳回),本版按核实成立的发现改写,修订记录见 §20。R1 起分批落码,每批按 AGENTS §10.1 交 Codex 编译测试。
>
> - **用户拍板(2026-10-19)**:国内、海外两边都要;先做苹果 + 谷歌这套通用骨架,国内渠道作为第二批接上;目前**没有任何开发者账号**,先用本地 `mock` 渠道把整条链路跑通。
> - **分工**(两会话协商,最终由用户拍板):共用骨架(支付服务、渠道接口、发货、退款扣回、本文)由会话「苹果和安卓应用充值」负责;微信 / 抖音小游戏渠道插件和小游戏登录由会话「微信抖音充值接入」负责。
> - **硬依赖**:本文建在**未合入 main** 的 `origin/snapshot/all-changes-20261018` 的商城发货 / 退款契约之上(§2.2)。该分支合入前,R1 只能做不依赖它的部分(§17)。

## 0. 一句话结论

**充值 = 买商城里的一个"官方固定价钻石包"。** 订单与钱包各只有一份权威(D-17):订单在 `java/mall_game`,钻石在 C++ `kCurrencyDiamond`。新增的只有 Go 全局服务 `go/payment`。它是商城唯一的 `payment` 角色调用方,只做五件事:

1. 给客户端发起付款所需的参数:苹果的 appAccountToken、谷歌的混淆号、微信的 signData/paySig 等;
2. 向平台**核实**付款,先把核实结果持久落进自己的库,再由后台推进任务调商城记"已付";
3. 接收平台的退款 / 撤销通知,并定时补漏;核实后调商城记"已撤销";
4. 执行商城要求的主动退款,只限有退款接口的渠道;
5. 未成年人充值限额闸(国内)。

发钻石、扣回钻石由 `go/trade` 的商城发货流程执行:拉商城待办 → 资产通道 → scene,所有"有没有发、要不要扣"的裁决都在 trade。

## 1. 目标与非目标

**目标**

- G1 iOS:App Store 消耗型 IAP 买钻石包,服务端核验 + 退款扣回。
- G2 安卓海外:Google Play 一次性消耗型商品,服务端核验 + 服务端 consume + 退款扣回。
- G3 国内安卓与小游戏:同一套骨架,渠道以插件接入(R3)。
- G4 任一环节崩溃、重试、重复通知、客户端掉线,**不丢单、不重复发货、不漏扣回**:同一笔平台交易最多入账一次;已付必达;已撤销必扣。
- G5 本地无账号也能端到端联调:`mock` 渠道 + robot `recharge-smoke`。

**非目标(v1 不做)**

- 首充双倍、月卡、订阅、礼包组合:v1 只卖"N 元 = M 钻石",每单数量恒为 1。
- 平台游戏币 / 平台钱包:微信小游戏游戏币模式(`wx.requestMidasPayment`,或 GameItem 的 `mode="coins"`)、抖音游戏币。它们和 D-17 的单一钱包冲突,**不接**,只用道具直购。
- 聚宝斋人民币寄售收款:以后作为同一骨架的另一种订单用途接入。本文接口不排除它,但 v1 不实现。
- 玩家提现、卖家结算。
- 客户端 TCP 改 WebSocket:小游戏形态的前置条件,不在本文范围。

## 2. 现状(2026-10-19 核实)

### 2.1 main 上

| 事实 | 证据 |
|---|---|
| 商城订单进程已有:幂等下单、库存预占、可信支付回执、取消;付款后只到 `DELIVERY_PENDING` | `java/README.md`;D-17.4 |
| 支付回执 `POST /internal/payments` 请求体 `{orderId, channel, transactionId[A-Za-z0-9_-]{1,128}, amountMinor, currency}`。main 上对已关闭订单回 409「订单已关闭」;**分支已改**(§2.2) | `mall/CommerceApi.java`;main `CommerceStore.java:117-140` |
| 商城错误体只有 `{"error": 中文}`:金额不符、已有不同回执、状态冲突、流水已用、nonce 重放全是 409,**没有机器可读的码** | `CommerceController.java:28-30`;`NonceStore.java:51` |
| 商城签名:`HMAC-SHA256(secret, "v1\nrole\nsubject\nts\nnonce\nMETHOD\n原始URI\nhex(sha256(body))")`,±120 秒,nonce 防重放。**角色按路径硬编码**,未登记的路径一律算 `gateway`,`gateway` 要求 subject 是玩家号 | `HmacAuthentication.java:36-71` |
| SKU 库存是有限 INT,下单预占、付款扣减;没有上架接口或种子脚本;取消会无条件释放预占(影响行数不为 1 就回 409) | `CommerceStore.java:73,99-111,130` |
| 每买家未付单上限 5,支付窗口 15 分钟,不自动关单 | `CommerceProperties.java:14-17`;`CommerceStore.java:48-49,69-71` |
| 钻石:`kCurrencyDiamond = 1`(另有金币 0、绑定钻石 2) | `cpp/libs/modules/currency/constants/currency.h:8-10` |
| `AddCurrency` 会先抵扣欠款(欠款被冻结或过期时不抵扣);`DeductCurrency` **完全不看欠款** | `currency_system.cpp:170-198,223-295` |
| `AttachDebt` **只记欠款、不扣余额**,而且会覆盖该币种已有欠款的 reason/operator/expiresAt;`WaiveDebt` 直接删除整条欠款 | `currency_system.cpp:327-414` |
| 资产通道调用方白名单只有 `guild`、`trade`;`TRADE_CREDIT` 只放行 `TX_AUCTION_BUY`/`TX_TRADE`,`TRADE_DEBIT` 只放行 `TX_AUCTION_SELL` 且余额不足按终局拒绝 | `asset_op_auth.cpp:43-52`;`asset_op_system.cpp:89-92` |
| 资产通道离线处理:scene 回 NOT_HERE 不记账,Go 侧退避后停放,玩家上线时唤醒重投 | `go/shared/assetop/caller.go:97-110`、`reconcile.go:670` |
| `PlayerInboxAdmin` 是运维面,所有方法共用一把 `x-admin-token`,生产方只有 GM;进程外生产方按设计推迟到收件箱第 5 批 | `player_inbox_admin.proto:10-51`;`player-inbox-service.md:734` §6.4 |
| `PaymentNodeService = 19`、`NODE_PAYMENT = 24` 只有枚举定义,没有任何使用 | `node.proto:24`;`proto_option.proto:43` |
| `EVENT_RECHARGE` 快照事件已定义,`CreateEventSnapshot` 零调用方 | `data_service.proto:433,438-450` |
| 实名 / 防沉迷 / 年龄:**零实现** | 全仓检索 |
| 聚宝斋设计里的 `Provider` / `BuyerEligibility` 只有文字,没有代码 | `jubaozhai-market.md:210-227` |
| 客户端:没有 `com.unity.purchasing`;Unity 6000.6.0f1;聚宝斋请求走 gate | `Packages/manifest.json`;`CommerceClient.cs:171` |
| 网关路由服只转发 gRPC | `microservice-zone-contract-20260914.md:186-266` |
| `docs/ops/deferred-clawback-bypass-audit-2026-05.md:58` 写的"充值 → Kafka → AddCurrency"**在代码里不存在** | 全仓检索 |
| `TransactionType` 当前最大值 29;30、31 在 main 与 snapshot 上都空闲(以落地时为准) | `transaction_log.proto:22-90` |

### 2.2 未合入的 `origin/snapshot/all-changes-20261018`(本文硬依赖,以它的语义为准)

| 内容 | 位置 |
|---|---|
| **已取消订单收到付款:回 200,state=`REFUND_PENDING`**,同一事务里把回执绑到原订单、写 `commerce_refund_intent`、订单置 5(不再是 main 的 409) | 分支 `CommerceStore.java:126-136`;`CommerceStoreTest.java:91` |
| 重放集合:状态属于 {1,2,5,6,7} 时同一回执重放回 200;其它非 0 状态回 409「订单状态冲突」;`(channel, txn)` 已被别的订单占用回 409 | 分支 `CommerceStore.java:126,137,151` |
| 发货待办 `POST /internal/deliveries/poll`、`/receipt`(`delivery` 角色)。poll 是**非破坏性**的,只 SELECT `status='PENDING'`,没有领取标记;receipt 只校验待办存在,条件更新不命中时静默成功 | 分支 `DeliveryStore.java:34-86`;`DeliveryController.java` |
| 退款待办 `POST /internal/refunds/poll`、`/receipt`(`payment` 角色),**目前没有调用方**;refunds 不分页;receipt 要求订单状态 5 | 分支 `DeliveryStore.java:87-126` |
| 发放物只支持物品:`commerce_sku_grant(sku_id, item_config_id>0, count_per_unit)`;`create()` 要求向 `commerce_order_grant` 恰好插 1 行,否则 409;poll 对它做 INNER JOIN;`/api/skus` 要求 `stock>lock_stock` | 分支 `V2__game_delivery.sql`;`CommerceStore.java:78`;`DeliveryStore.java:27-42` |
| `go/trade` 的 `Mall`:客户端 `ListOfficialSkus / CreateOfficialOrder / 查单 / 取消` 代理到商城(`gateway` 角色);`Tick` 拉发货待办 → `accept` 以 `RequestKey("official", player, orderId)` 建 `TradeCommerceRecord`(同摘要返回旧记录,摘要不同回 `ErrReplay`)→ Engine 独立推进 `official_credit` 腿 → 只在 Settled/8/Manual 时回执。**任意一条出错,整批 Tick 中断** | 分支 `mall.go:132-281`;`engine.go:448-460,768-816` |
| `official_credit` 腿没 applied(REJECTED)→ `official_refund`、记录 Status=8 → 回执 `REFUND_PENDING` → 商城写退款待办;只有 APPLIED_PARTIAL 才转 Manual | 分支 `engine.go:448-460,684-690`;`mall.go:264-271`;`DeliveryStore.java:81-84` |
| **tx_type 按流写死**:流 4 的所有腿一律 `TX_AUCTION_BUY`(流 3 是 `TX_AUCTION_SELL`);Workflow / Leg 上没有 tx_type 字段;资产通道 `correlation_id = trade_asset_op.op_id`,`ref_id = commerce_id` | 分支 `engine.go:744-756`;`trade_table.proto:205-236`;`asset_op_repo.go:764-768` |
| `PlayerAssetSQLCommit`:go/db 在玩家资产与账本同一 SQL 行提交后发布的提交证据,用来补 D-17.2 第 4 条"durable 只到 Redis"的缺口 | 分支 `asset_op.proto` |
| mall_game 没有迁移执行器:`schema.sql` 是全新安装的事实源,`V2` 是手工升级 SQL;`SchemaVerifier` 写死 version==1 并按清单核对表、列、唯一键 | 分支 `SchemaVerifier.java:21-25` |

**待用户确认**:这个分支由谁负责、何时合入 main。

## 3. 总体架构

```
                 ┌──────────── 平台服务器(苹果 / 谷歌 / 微信 / 抖音 / 渠道)────────────┐
                 │  核验 / 查单 / 通知历史 API                 通知(公网 HTTPS 推送)    │
                 └───────▲──────────────────────────────────────────┬─────────────────┘
                         │ 核实、补漏                                 │ 到账 / 退款 / 撤回
  ┌────────┐  ②建意图 ┌──┴─────────────────────────┐                  ▼
  │ 客户端 │────────▶│ go/payment(新,全局)        │  ④已付 / 撤销 / 撤回 / 无原单入账
  │ Unity  │  ③交收据 │ 渠道插件 · 本地事实库        │─────────(payment 角色)─────────▶┌───────────────┐
  │/小游戏 │────────▶│ 推进任务 · 合规闸            │◀── 只读查单 / SKU、退款待办 ───────│ java/mall_game │ 订单唯一权威
  └───┬────┘          └─────────────────────────────┘                                 └──────┬────────┘
      │ ①下单 / 查单 / 取消(经 gate,gRPC)                                                   │ 发货 / 撤销待办
      ▼                                                                                       ▼
  ┌──────────────┐  ⑤入账 TRADE_CREDIT(TX_RECHARGE)                     ┌──────────┐
  │ go/trade Mall │─────────────────────────────────────────────────────▶│ C++ scene │ 钻石唯一权威
  │ (已有,扩展)  │  ⑥扣回 TRADE_DEBIT(TX_RECHARGE_REVERSAL:先扣余额,差额记充值欠款) │          │
  └──────────────┘                                                        └──────────┘
```

| 组件 | 职责 | 不负责 |
|---|---|---|
| `go/payment`(新,`PaymentNodeService=19`,全局) | 客户端支付协议(建意图、交收据);渠道核验;平台通知入口与补漏;本地事实库 + 推进任务(向商城报已付 / 撤销 / 撤回,谷歌 consume);执行商城退款待办;合规闸 | 订单、库存、发货、扣回、钱包 |
| `java/mall_game`(扩展) | 订单与回执唯一权威;充值 SKU(货币发放物、无限库存、数量恒 1);迟到付款复活;无原单入账;撤销 / 撤回事实与待办;payment 角色只读接口 | 核验平台收据;判断发没发 |
| `go/trade` Mall(扩展) | 下单 / 查单 / 取消的客户端门面;发货;**撤销裁决与扣回、撤回补发** | 判断付没付 |
| C++ scene | `AddCurrency`;新增充值扣回原语;资产通道账本幂等 | — |

**为什么支付服务用 Go,不放进 mall_game**:①网关只转发 gRPC,客户端要直连它;②`noderegistry`、gate 路由、部署门禁只有 Go 设施;③D-17.2 第 3 条要求支付回调有独立的可信边界。

**苹果没有官方 Go 库**,JWS 验签用标准库按 §5.3 的清单自行实现,并用苹果官方 Java 库生成的 golden 向量对拍(§16)。

## 4. 商品与钱包模型

- **钻石包 = 商城官方 SKU,且必须同时满足:货币发放物、无限库存、每单数量恒为 1。** v1 默认档位(用户可改):

  | SKU 标价(CNY 分) | 钻石 | 苹果 / 谷歌商品 ID |
  |---|---|---|
  | 600 | 60 | `diamond_60` |
  | 3000 | 300 | `diamond_300` |
  | 6800 | 680 | `diamond_680` |
  | 12800 | 1280 | `diamond_1280` |
  | 32800 | 3280 | `diamond_3280` |
  | 64800 | 6480 | `diamond_6480` |

- 一个 SKU **物品发放物与货币发放物恰好一种**,建 SKU 与下单时都要校验。货币发放物在下单时快照进订单,防止改表影响已付订单。
- 充值 SKU:
  - 下单时 `quantity != 1` 回 400;
  - 不预占、不扣库存,取消也不释放预占;
  - **不计入每买家未付单上限**;
  - 超过支付窗口由商城自动关单。关单是安全的,因为迟到付款会复活订单(§10.1)。
- 客户端对同一 SKU **复用自己尚未付款的订单**。遇到苹果"家长批准(Ask to Buy)"或谷歌待付款(PENDING)时保留原单、不主动取消。
- 渠道商品映射:`go/payment` 配置 `sku_id ↔ (channel, store_product_id)`。启动时用 payment 角色的只读 SKU 接口核对:映射的 SKU 存在、已上架、是充值 SKU。不一致拒启。
- 钱包只有 `kCurrencyDiamond`。**不发绑定钻石**;赠送类以后另行设计。
- 玩家处于货币获取封禁(`BlockCurrency` 或全局 `GainBlock`)时,`CreatePayIntent` 直接拒绝。已经付了款的走人工,或解封后重投(§10.6)。

## 5. 主流程:苹果 / 谷歌(收据式)

以"玩家 1001 买 diamond_60,6.00 元"为例。

### 5.1 步骤

| 步 | 谁 | 做什么 | 幂等 / 失败处理 |
|---|---|---|---|
| ① | 客户端 → gate → go/trade | `CreateOfficialOrder{sku_id, quantity=1, client_request_id}` → 商城建单 `A123`,状态 `AWAITING_PAYMENT` | 商城 `(buyer, request_id)` 唯一 |
| ② | 客户端 → go/payment | `CreatePayIntent{order_id, channel}`。go/payment 用 payment 角色只读接口查单,要求:买家 = 会话玩家、状态 `AWAITING_PAYMENT`、数量 1、SKU 有该渠道映射、玩家未被封禁货币获取;然后过合规闸(§12,预占额度) | **一个订单只允许一个渠道的意图**,要换渠道就取消原单重下。token 由订单号确定性派生,重复请求返回同一意图 |
| ② 返回 | | 苹果:`app_account_token = UUID(HMAC(K_bind, "apple:" + order_sn))`。谷歌:`obfuscated_account_id = base64url(HMAC(K_obf, player_id))`(43 字符),`obfuscated_profile_id = base64url(HMAC(K_bind, "google:" + order_sn))`(43 字符)。两段式渠道见 §6 | 都不含个人信息,也不暴露内部订单号;谷歌两项都 ≤ 64 字符 |
| ③ | 客户端 | Unity IAP 拉起付款。苹果先调 `SetAppAccountToken`(状态型 setter),**购买流程串行**,上一笔回调完才允许下一笔 | |
| ③' | 客户端 → go/payment | 付款回调里把苹果 `jwsRepresentation` 或谷歌 `purchaseToken + productId` 交给 `SubmitStoreReceipt` | 限流(MessageLimiter) |
| ③核实 | go/payment → 平台 | 见 §5.3 | 平台不可达 → 可重试错误,`finish_transaction=false` |
| ③落库 | go/payment | 把核实结果写进 `payment_store_txn`(状态机见 §8.1),**事务提交后**返回 `finish_transaction=true`,客户端随即 `ConfirmPurchase()` | 不管结果是"待入账""孤儿""人工"还是"被拒",只要已经持久落库,就让客户端结束交易 |
| ④ | 推进任务 → 商城 | 后台扫描"已核实未入账"的记录,调 `POST /internal/payments`,`amountMinor/currency` = 订单金额(数量恒 1,即 SKU 标价)。推进前先 CAS 确认本地未撤销 | 超时 / 5xx / 网络错误按退避重试,每次重试重新生成 nonce。返回 200 时按 `state` 分流:`DELIVERY_PENDING / DELIVERED / MANUAL` → 已入账;`REFUND_PENDING` 对充值 SKU 不应出现,出现就告警 + 人工。409 按机器码分流(§8.2.7) |
| ④' | 推进任务 → 谷歌 | 落库后调 `purchases.products.consume`(同时算确认) | 3 天内必须完成;对方回"已消耗"按成功处理;有"超过 48 小时未 consume"告警 |
| ⑤ | go/trade Mall.Tick | 拉商城发货待办,建 `TradeCommerceWorkflow`,`official_credit` 腿 `Currencies[{1, 60}]`,腿上 `tx_type = TX_RECHARGE` | `RequestKey("official", player, order_sn)` 唯一 |
| ⑥ | go/trade → scene | 资产通道 `TRADE_CREDIT`,`correlation_id = trade_asset_op.op_id`。对账链:`ref_id = commerce_id` → 合同的 `ExternalOrderSn` → 商城订单 | 账本 `(player, stream, epoch, seq)` 幂等;离线停放、上线唤醒 |
| ⑥ 回执 | go/trade → 商城 | 拿到资产持久证明后回执 `DELIVERED`;腿被拒时回执 `MANUAL`(§10.6) | 没拿到持久证明就保持"交付中",不报完成 |
| 展示 | 客户端 | 轮询订单直到 `DELIVERED`,然后刷新钻石 | |

### 5.2 三条到账路径殊途同归

三条路径都汇入同一个 `ConfirmStoreTransaction(channel, transaction_id)`,它只从"已核实"状态做 CAS 推进:

1. 客户端交收据(③');
2. 平台主动通知:苹果 `ONE_TIME_CHARGE`,谷歌 RTDN 的一次性商品购买通知;
3. 客户端每次启动用 `FetchPurchases()` 把未结束的交易重交一遍。

这样,客户端付完款就崩溃,或谷歌"慢速付款"几小时后才成功,服务端都能靠通知入账。

**通知回包时机**:只有在事实已写进 `payment_store_txn` / `payment_notification` 且事务已提交之后,才回 200 或 ack。验签失败、落库失败一律回非 2xx,让平台重投。

### 5.3 核实清单

**苹果**:

- 客户端交来的 JWS **只用来取 transactionId 和做预检**。
- 唯一入账依据是 go/payment 按交易环境调 Get Transaction Info 拿回的 `signedTransactionInfo`。它同样要完整验签,`transactionId / productId / appAccountToken / bundleId / environment / quantity / revocationDate` 都以它为准。它和客户端 JWS 不一致就拒绝并告警。
- 本地验签规格对齐官方库:
  1. `header.alg` 只认 `ES256`;
  2. `x5c` 恰好 3 张;
  3. 信任锚固定为 Apple Root CA - G3,写死指纹;
  4. 叶证书必须带扩展 OID `1.2.840.113635.100.6.11.1`,中间证书必须带 `1.2.840.113635.100.6.2.1`(只验"能链到苹果根"不够,G3 下面还有别的用途的证书);
  5. 用 `signedDate` 校验证书有效期;
  6. 核对 `bundleId`、`environment`,生产环境再核对 `appAppleId`。
- 不得为绕开扩展密钥用途检查而放宽成 `ExtKeyUsageAny` 却不补上述 OID 检查。

**谷歌**:

- 调 `purchases.productsv2.getproductpurchasev2`,核对:状态为已购买、商品、`obfuscatedExternalAccountId` 等于会话玩家的派生值、`obfuscatedExternalProfileId` 命中意图、`productOfferDetails.quantity == 1`。
- `testPurchaseContext` 存在即测试购买(§10.5)。
- 实收金额:核实后用 `orderId` 异步调 Orders API(`orders.get`)补写,不阻塞入账。

**通用**:

- 收据里的 token 没命中意图,或命中的意图商品与交易商品不一致 → 孤儿(§5.4)。
- 数量不等于 1 → 人工。
- 已撤销(`revocationDate` 存在,或谷歌状态非已购买)→ 记为已撤销,不入账。

### 5.4 孤儿与"一个订单收到第二笔钱"

- **孤儿**:收据里没有 token,或 token 对不上任何意图。处理:
  - 不自动入账,进人工队列并告警。
  - 人工绑定 `ResolveOrphan` 的要求:
    - 绑定前实时向平台复核未撤销;
    - 用 `txn_key` 唯一约束防止重复绑定;
    - 双人审批,同事务写审计;
    - 必须有平台侧的账号证据,例如谷歌混淆账号号能反查到玩家。客户自己提供的收据不能作为唯一归属依据。
  - 绑定后走"无原单入账"(§8.2.4)。
  - 原因:token 是"这笔钱是哪位玩家付的"的唯一可信来源。拿交收据的会话玩家顶替,会让截获别人收据的人抢先入账。
- **同一订单收到第二笔已核实交易**:客户端复用了 token,或双设备并发。商城回 409「已有不同回执」时:
  - 只要交易的 token 派生自该玩家的订单、商品一致,就**自动走"无原单入账"**:为同一买家、同一 SKU 建新单并发货,幂等键 `(channel, txn)`。
  - 照常计入合规累计;超限则转人工、不自动发货(§12)。
  - 有指标和告警,用来发现客户端缺陷。

## 6. 两段式渠道(小游戏 / 微信支付 / 支付宝 / 国内渠道)

接口按会话「微信抖音充值接入」核实的结果预留,平台事实见附录 A。

- **建意图由服务端签名**:
  - `CreatePayIntent` 带 `client_params`(map),由渠道插件读取。例如微信小游戏:客户端付款前调一次 `wx.login`,把 code 放进 client_params。
  - 渠道插件生成签好名的客户端参数,通过 `channel_params`(map)返回,客户端原样转交给平台前端 API:
    - 微信小游戏道具直购:前端 API `wx.requestMidasPaymentGameItem`,`signData` 中 `mode="goods"`。由服务端生成整串 `signData`(必须逐字节一致)、`paySig`、`signature`。
    - 抖音小游戏:安卓 `tt.requestGamePayment`(`goodType=2`),没有服务端签名,只回参数。
- **session_key 按需现取**:微信插件用 code 当场调 `jscode2session` 拿到新的 session_key 和 openid,用完即弃,**不落库**。现取得到的 openid 必须与 `payment_channel_identity` 里登录时登记的一致,否则拒绝。
- **到账以平台推送为准**:
  - 每个渠道一条公网回调路由 `/notify/<channel>/<appid>`。
  - `ParseNotification` 允许返回"只回包、不产生事件"的结果:配置握手(微信 `echostr`、抖音 GET 校验)、模拟推送(微信 `IsMock=true`)、非支付事件。
  - 按平台格式和时限回包(微信 3 秒,抖音只要 HTTP 200)。
  - 同一平台单号的重复推送,回和第一次相同的成功。
  - 微信小游戏的推送地址每个 appid 只能配一个,以后可能还要分发客服消息等其它事件,所以路由**不写死成"只有支付"**。
- **主动查单兜底**:定时扫描未决意图。查单接口要用户签名的渠道(微信 `queryOrder` 待核实)不能后台查,降级为靠推送重试 + 日对账。
- **只做道具直购**:一个 SKU = 一个固定价钻石包;游戏币模式不接。

## 7. 渠道插件接口(Go)

```go
// 一个渠道 = 一个实现。骨架只依赖这个接口;渠道私有的密钥、证书都在实现内部,按环境(production/sandbox)各一组。
type Channel interface {
    Name() string // "mock" / "apple" / "google" / "wechat_minigame" / "douyin_minigame" / ...

    // 建意图:收据式渠道只返回绑定用的 token;两段式渠道返回签好名的参数。
    // in.ClientParams 是客户端附带的参数(例如微信 wx.login 的 code),由插件自行解释。
    CreateIntent(ctx context.Context, in IntentInput) (IntentOutput, error)

    // 核实客户端交来的收据。返回值只来自平台核验结果,不得回显客户端字段。
    VerifyReceipt(ctx context.Context, in ReceiptInput) (VerifiedTxn, error)

    // 解析并验签平台通知。验签失败返回 ErrBadSignature,调用方不落任何状态并回非 2xx。
    // events 可以为空:握手、模拟推送、非支付事件只回包、不产生事件。
    // Ack 是给平台的回包体;调用方在事件落库并提交之后才发出它。
    ParseNotification(r *http.Request) (events []Event, ack Ack, err error)

    // 主动查单兜底。需要用户签名才能查的渠道返回 ErrQueryUnsupported,骨架降级为只靠推送和对账。
    Query(ctx context.Context, ref TxnRef) (VerifiedTxn, error)

    // 发货确认:谷歌 consume、华为消耗;微信是推送回包失败时的"补通知已发货"(待核实)。不需要的渠道返回 nil。
    Finalize(ctx context.Context, txn VerifiedTxn) error

    // 主动退款。苹果、微信小游戏、抖音小游戏不支持开发者主动退款,返回 ErrRefundUnsupported。
    Refund(ctx context.Context, req RefundRequest) (RefundResult, error)
}

type VerifiedTxn struct {
    Channel        string
    TransactionID  string // 平台交易号原样保存;发给商城前经 §10.4 规范化
    StoreProductID string
    Quantity       int64  // 必须为 1,否则进人工
    IntentBinding  string // 苹果 appAccountToken / 谷歌 obfuscatedProfileId / 两段式渠道的商户订单号
    AccountBinding string // 谷歌 obfuscatedAccountId / 小游戏 openid;其它渠道可空
    ListAmount     int64  // 平台给出的标价(微信 OrigPrice);商店定价渠道填 0
    PaidAmount     int64  // 平台实收,最小单位;拿不到填 0,并把 PaidKnown 置为 false
    PaidCurrency   string
    PaidKnown      bool
    Environment    string // production / sandbox。苹果取 JWS;谷歌:有 testPurchaseContext 即 sandbox;微信取推送 Env
    State          TxnState // Paid / Pending / Revoked
    RevokedAtMs    int64
}
```

`mock` 渠道:

- 只在 go-zero 顶层 `Mode ∈ {dev, test}` 时允许注册,否则拒启;`MockNotify` 运行时再核一次 Mode。
- 收据格式 `mock:<order_sn>:<nonce>`,核实恒成功。
- 内部管理 RPC `MockNotify` 可模拟到账、退款、撤回,供 robot 与 GM 使用。它不是客户端协议。

## 8. 数据与改动面

### 8.1 go/payment 自己的库 `mmorpg_payment`

表源是 proto,建表走 `go/schemamigrate`,遵守 D-14。约束:主键用 `shared/idsegment` 发的整数;每表至多一个唯一键;键列为整列 `VARCHAR(N) utf8mb4_0900_bin`,proto 上标 `max_length`,应用层先行限长。

| 表 | 用途 | 唯一键(最大长度) |
|---|---|---|
| `payment_intent` | 意图:order_sn、player_id、channel、sku_id、合规预占额、状态(有效 / 已用 / 已过期 / 已取消)、过期时间 | `binding_key` = channel + ":" + 派生 token(≤ 80)。token 由订单号确定性派生,所以"一个订单一个渠道一条意图"由这个唯一键直接保证 |
| `payment_store_txn` | 平台交易事实:原始交易号、规范化交易号、商品、数量、标价 / 实收与币种、环境、测试标记、核验摘要、意图 id、状态 | `txn_key` = channel + ":" + 规范化交易号(≤ 161) |
| `payment_notification` | 通知去重与审计,只存摘要,不存整包 | `notification_key` = channel + ":" + 通知 UUID(≤ 161) |
| `payment_channel_identity` | 渠道身份映射:openid ↔ player,由登录侧写入 | `identity_key` = channel + ":" + appid + ":" + openid(≤ 191) |
| `payment_spend` | 合规累计,按实名身份统计(§12) | `spend_key` = 身份哈希 + ":" + 周期(yyyymm 或 yyyymmdd)(≤ 96) |
| `payment_audit` | 人工绑定、人工重推、人工退款的双人审批与审计 | 无 |

**`payment_store_txn` 状态机**:每次迁移都在行锁或 CAS 下完成。

```
                ┌──────────────(撤销先到:占位)──────────────┐
                ▼                                              │
  [无记录] ─核实─▶ VERIFIED ──推进成功──▶ POSTED ──finalize──▶ FINALIZED
                    │  │                    │                    │
                    │  └─孤儿/超限/异常─▶ MANUAL ──人工处理──▶ POSTED / REFUNDED
                    │                                           │
                    └──撤销──▶ REVOKED(终态,不再推进)        └──撤销──▶ REVERSAL_PENDING ─商城已记──▶ REVERSED
                                                                     REVERSED ──撤回──▶ REINSTATE_PENDING ─商城已记──▶ POSTED
```

- 撤销到达时本地是 VERIFIED / MANUAL(还没在商城入账):直接置为 REVOKED,之后的推进一律拒绝。
- 撤销到达时本地没有记录:落一条 REVOKED 占位。后到的收据看到占位一律拒绝。
- 撤销到达时本地是 POSTED / FINALIZED:置为 REVERSAL_PENDING,由推进任务调商城 reversal,失败就持久重试。
- `ConfirmStoreTransaction`、人工重推、`ResolveOrphan` 都只能从 VERIFIED 出发做 CAS,推进前必须实时向平台复核未撤销。

这里**不存订单状态,也不存余额**:订单状态只在商城,余额只在 scene。

### 8.2 java/mall_game(V3,依赖 snapshot 分支的 V2)

形态照 V2 先例:`schema.sql`(全新安装)与 `V3__recharge.sql`(升级)同批加表,`SchemaVerifier` 加表清单与关键唯一键检查。理由援引 D-17.1"裁剪 / 扩展后的独立存储合同"。Java 仓 `game-mall` 同批对齐(§19)。

1. **新表**:`commerce_sku_currency_grant(sku_id, currency_type, amount_per_unit)`、`commerce_order_currency_grant`、`commerce_sku_unlimited(sku_id)`、`commerce_reversal`(撤销 / 撤回事实与待办,带 eventId 与序号)。
2. **发放物二选一**:
   - `create()` 改为物品与货币恰好一种,缺失回 409;
   - 充值 SKU 强制 `quantity=1`;
   - 充值 SKU 跳过预占与未付单上限;
   - `cancel()` 和付款对无限库存 SKU 都跳过 `lock_stock/stock` 运算;
   - `skus()` LEFT JOIN 两种发放物,无限库存 SKU 的 `available` 定义为一个固定上限;
   - `poll()` LEFT JOIN 两种发放物快照并校验恰好一种,`DeliveryApi.Intent` 增加 `currencyType/currencyAmount`;
   - `DeliveryApi.Sku` 和 `OfficialSkuView` 返回发放物类型和无限库存标记。
3. **迟到付款复活**:改 `recordTrustedPayment` 的 status==4 分支。对充值 SKU,在同一事务里把订单从 4 重开到 1,回执仍绑在原订单上,写发货待办,**不写 refund_intent**。物品 SKU 保持分支现有语义。
4. **无原单入账** `POST /internal/payments/unbound`(payment 角色):
   - 请求体 `{buyerId, skuId, channel, transactionId, amountMinor, currency, source: EXTRA|ORPHAN_RESOLVED}`;
   - 在一个事务里建单、记回执、写发货待办,幂等键 `(channel, transaction_id)`;
   - 只对充值 SKU 开放。
5. **撤销** `POST /internal/payments/reversal` 与**撤回** `/internal/payments/reinstate`(payment 角色):
   - 请求体 `{channel, transactionId, eventId}`;
   - 定位不到 `(channel, txn)` 时返回明确的"未知交易"码;
   - 订单进新状态 `REVERSED`,或从 `REVERSED` 回到原状态;
   - 同事务写撤销 / 撤回待办。**商城不作废发货待办,也不裁决发没发**;
   - `REVERSED` 加入 `:126` 的重放集合、`view()`、`list()`,以及 trade 的 `mallAnswer` 和客户端的 `CommerceOrderState`(客户端契约,按 AGENTS §12 处理)。
6. **撤销待办拉取 / 回执** `/internal/reversals/poll`、`/receipt`(delivery 角色,go/trade 消费,带游标)。另外:
   - `receipt()` 对已 `REVERSED` 订单收到的 `DELIVERED` 回执不能静默吞掉,要确认对应撤销待办已存在;
   - 新增发货回执状态 `VOIDED`(trade 判定不发)。
7. **机器可读错误码**:错误体增加 `code`,区分 `AMOUNT_MISMATCH / DIFFERENT_RECEIPT / STATE_CONFLICT / TXN_USED / NONCE_REPLAY / UNKNOWN_TXN`,go/payment 按 code 分流,不匹配中文文案。
8. **payment 角色只读接口**:`/internal/payments/order-view`(按 orderId 返回买家、SKU、数量、金额、状态)、`/internal/payments/skus`(返回充值 SKU、货币发放物、上架状态、无限库存标记)。
9. **退款待办**:
   - 加来源枚举(`CANCELLED_THEN_PAID` 等),go/payment 只自动执行白名单来源,其余转人工;
   - 加一种关闭回执"渠道不支持退款,已转发货";
   - `refunds()` 加游标;
   - 对充值 SKU 拒收 trade 的 `REFUND_PENDING` 回执。
10. **路由 → 角色表**(`HmacAuthentication`):
    - `payments/unbound|reversal|reinstate|order-view|skus` 归 payment;
    - `reversals/poll|receipt` 归 delivery;
    - 对应 Controller 再核一次角色;
    - 补测试钉住"未登记的 `/internal` 路径回 404""错角色回 401"。

### 8.3 go/trade(依赖 snapshot 分支)

1. **tx_type 下沉到腿**:`TradeCommerceLeg` 新增 `tx_type`。充值合同的 `official_credit` 腿与补发腿填 `TX_RECHARGE`,扣回腿填 `TX_RECHARGE_REVERSAL`。`Engine.advance` 优先取腿上的值,为 0 时退回按流取值。改 `proto/trade/trade_table.proto` 并重新生成。
2. **货币发放**:`Mall.accept` 按待办建 `Items` 或 `Currencies`。摘要对新增字段保持兼容:零值不进摘要,或摘要带版本号。
3. **充值腿被拒不退款**:`Engine.plan` 对 `tx_type=TX_RECHARGE` 的 `official_credit` 在 REJECTED 时转 Manual、回执 `MANUAL`,不进 `official_refund`。
4. **撤销裁决**(新的撤销消费者)。"发没发"只由 trade 按自己库里的记录判定:
   - (a) 没有 `RequestKey("official", player, order_sn)` 记录:用同一个键插入 `VOIDED` 墓碑。之后 `accept` 撞键看到墓碑,返回"已作废"(不是 `ErrReplay`),回执商城 `VOIDED`。
   - (b) 有记录、`official_credit` 腿还没终结:撤销挂起,等腿终结。applied 就扣回,没入账就什么都不做。玩家离线停放期间撤销先到,也走这条。
   - (c) 已入账:直接扣回。
   - 扣回入口:`RequestKey("official_reversal", player, order_sn + "#" + reversalSeq)`,建 `TRADE_DEBIT` 腿,`tx_type = TX_RECHARGE_REVERSAL`,金额 = 当初的发放量。
   - 撤回补发入口:`RequestKey("official_reinstate", player, order_sn + "#" + reinstateSeq)`,建 `TRADE_CREDIT` 腿,`tx_type = TX_RECHARGE`。
   - 两个入口各用独立的作用域,带上商城的事件序号,同一订单"退款 → 撤回 → 再退款"每次都能执行。
5. **Tick 不再整批中断**:发货与撤销两个消费者遇到单条非法、冲突或未知形状的待办时,跳过、计数、告警、转人工。
6. 发货成功后调一次 `data_service.CreateEventSnapshot(EVENT_RECHARGE)`,失败只告警不阻塞。

### 8.4 proto 与 C++

- `transaction_log.proto`:新增 `TX_RECHARGE = 30`(真钱充值入账,含撤回补发)、`TX_RECHARGE_REVERSAL = 31`(退款扣回)。号码以落地时为准。
- `asset_op_system.cpp` 流规则:`TRADE_CREDIT` 放行 `TX_RECHARGE`;`TRADE_DEBIT` 放行 `TX_RECHARGE_REVERSAL`。**不新增调用方、不新增流**,发货与扣回都由 trade 执行。
- **充值扣回原语**:`TX_RECHARGE_REVERSAL` 的 DEBIT 语义与普通扣款不同。scene 在一次原子处理里:
  1. 先扣 `d = min(钻石余额, 应扣)`;
  2. 差额 `应扣 − d` 记成**充值欠款**;
  3. `d` 与差额写进账本结局,供补发时反向处理。
- **充值欠款单独成条**:
  - 与 GM 欠款分开存,例如 `CurrencyComp` 的欠款增加来源字段;
  - 不受 `WaiveDebt / FreezeDebt / expiresAt` 影响,也不覆盖 GM 欠款;
  - 补发(`TX_RECHARGE` 且关联到撤回)时优先核销这一条,再把当初从余额扣掉的部分加回;
  - 普通 `AddCurrency` 照常先抵扣它。
  - 改动落在 `currency_system`(新函数)、`currency_comp.proto`、`asset_op_system.cpp`。
- `proto/payment/payment.proto`:客户端协议 service(`OptionIsClientProtocolService`),方法 `ListPayChannels / CreatePayIntent / SubmitStoreReceipt / GetPayIntent`。
- `proto/payment/payment_admin.proto`:内部管理 service,方法 `MockNotify / ResolveOrphan / ListManual / RetryManual`。
  - 鉴权:独立 env 令牌,≥ 32 字节,只放 gRPC metadata;
  - NetworkPolicy 只放行运维工具。
  - 客户端 `gen_proto.ps1` 不收这个文件。
- `proto/payment/payment_table.proto`:§8.1 的表。
- tip 码:`Tip.xlsx` 新开组头 `//payment_error base=<落地时选定> width=1000`,在 `tip-code-axis.md` 登记,同步 `cpp/generated/table/CMakeLists.txt` 与 `table.vcxproj`(AGENTS §4.4)。

### 8.5 部署与接入清单(R1 逐项落地,每项配契约测试)

1. `proto_gen.yaml` 与 `proto_gen.no-unity.yaml` 都加 payment 块。漏了整个域不被解析、消息号丢失(DV-5)。
2. `$ProtoGenPostHooks` 加 `payment_postgen.py`(register-cpp / vendor-robot)。
3. D-14 建库建表:
   - `payment-migrate` Job;
   - `00_init_zone_dbs.sql` 建 `mmorpg_payment` 并授权;
   - 连接总账及其契约测试;
   - `go.mod` 的 proto2mysql require/replace。
4. 号段 biz_tag 四处同步,先跑 `data_service -migrate`。
5. 路由与启动脚本:路由表、`Assert-RouterForwardBudget`(Timeout ≤ 4000)、`go_services.ps1`、`go_svc_image.ps1`、go-svc manifest、`start_game.ps1`。
6. `k8s_deploy.ps1`:staging/prod 固定 `Mode: pro`,并加契约测试。
7. 通知入口单独端口 + 单独 Ingress,只暴露 `/notify/*`;微信要求 80/443。
8. 发布顺序:
   - 基础设施侧:`data_service migrate` → `payment-migrate` → `go/payment` → 路由服 → gate;
   - 商城侧:trade 先上(能识别货币待办、新状态,会跳过未知待办),商城 V3 后上。
9. **`java/mall_game` 的 K8s 部署**(manifest、Secret、Service 地址)目前没有,单列为 R1 前置依赖。

### 8.6 密钥持有表

| 进程 | 商城密钥 | 其它 |
|---|---|---|
| go/payment | 只持 `payment` 一把 | 苹果 `.p8` + Issuer ID + Key ID;谷歌服务账号 JSON;`K_bind`、`K_obf`;各渠道商户密钥(按环境各一组);管理面令牌 |
| go/trade | `gateway` + `delivery` | 资产通道 `MMORPG_ASSET_OP_SECRET_TRADE` |

`payment` 密钥本身就能给任意待付订单记"已付",爆炸半径等于"免费发钻石"。缓解措施:

- 只经 K8s Secret 注入 go/payment;
- NetworkPolicy 只允许 go/payment 访问商城的 `/internal/payments*`;
- 商城侧对 `unbound` 限速并告警;
- 每日三方对账(§15)。

## 9. 退款与扣回

| 来源 | 平台事件 | go/payment 动作 | 结果 |
|---|---|---|---|
| 苹果 | `REFUND` / `REFUND_REVERSED` / `ONE_TIME_CHARGE` | 通知只当触发:按 transactionId 调 Get Transaction Info,以返回的 `revocationDate / revocationReason` 为准,推进本地状态机(§8.1),再调商城 reversal / reinstate / 入账 | trade 扣回 / 补发 / 发货 |
| 苹果 | `REFUND_DECLINED` | 只记录 | — |
| 苹果 | `CONSUMPTION_REQUEST` | v1 只记录、不回传(§18.3) | 苹果自行判定 |
| 苹果 | **Get Notification History**(每小时补漏,按类型分页拉最近窗口) | 用 `notification_key` 去重后进同一处理链 | 补上推送链路漏掉的退款 |
| 谷歌 | RTDN `voidedPurchaseNotification` | 调 API 查实 → reversal | 扣回 |
| 谷歌 | Voided Purchases API(每日兜底,近 30 天,含拒付,显式带 `includeQuantityBasedPartialRefund=true`) | 同上;按数量的部分退款进人工 | 补漏 |
| 小游戏 | 微信 `minigame_pay_refund_succ_notify` 等 | 同上 | 扣回 |
| 商城退款待办 | `/internal/refunds/poll` | 只执行白名单来源。有退款接口的渠道(谷歌 `orders.refund`、微信支付等)调 `Channel.Refund`,调用前再查一次订单,确认从未产生发货回执。不支持退款的渠道回执"已转发货" | 不吞玩家的钱 |

**扣回语义**:先扣钻石余额,不够的差额记成充值欠款,以后获得钻石时自动补缴(§8.4)。例子:充 6480 钻后退款,玩家已花掉 6000、剩 480:扣掉 480,记 6000 欠款。是否附加封号或禁充值由运营另定(§18.4)。

## 10. 边界决定

**10.1 迟到付款**:玩家已付给苹果、微信小游戏、抖音的钱无法由我们退回,所以"订单已取消后付款到账"对充值 SKU **一律发货**:商城在 V3 里把订单从 4 复活为 1(§8.2.3)。go/payment 不需要单独处理,照常推进即可。能退款的渠道也一样复活发货,避免玩家看到"付了钱却被退款"。

**10.2 价格**:

- **商店定价**的渠道(apple / google / huawei 等)按各店面本地价收钱,与 SKU 标价不可能处处相等。go/payment 发给商城的金额 = 订单金额(数量恒 1,即 SKU 标价),商城的金额校验继续防"映射到错误的 SKU"。实收金额币种另存,供财务对账;谷歌实收用 Orders API 异步补。
- **我们定价**的渠道:
  - 微信小游戏:推送里的 `OrigPrice` 必须等于 SKU 标价;`ActualPrice` 可能因平台优惠低于标价,只存不校验。
  - 抖音小游戏:金额来自前端参数,可能被篡改,回调里的 `amount_cent` 必须等于标价。
  - 微信支付 / 支付宝:实收必须等于标价。
  - 不符的拒入账并告警。

**10.3 一一对应**:

- 一个意图只绑一个订单;一个订单只有一个渠道的意图。
- 同一笔平台交易只能入账一次:两层保证,`payment_store_txn.txn_key` 加商城 `(channel, transaction_id)` 主键。
- 订单已被别的交易付过时,按 §5.4 走"无原单入账"。

**10.4 交易号规范化**:商城要求 `[A-Za-z0-9_-]{1,128}`。

- 苹果 `transactionId` 是纯数字,原样使用。
- 谷歌 `orderId` 形如 `GPA.xxxx-…`,含点号,`purchaseToken` 又可能过长,所以一律用 `hex(sha256(purchaseToken))`(64 字符),原始 orderId 与 token 存在 `payment_store_txn`。
- 微信小游戏用我们生成的 `OutTradeNo`(≤ 32 字符,平台侧交易号不保证返回)。
- 抖音用 `cp_orderno`(即我们的 customId),平台单号 `order_no_channel` 另存。
- 规则固定在各渠道实现里,写单测锁死。

**10.5 测试交易与环境**:按苹果官方做法,**生产服务器也要能核验沙盒交易**,因为审核员用正式包、走沙盒付款。go/payment 按交易自身的 environment 调对应域名。

- 判定:苹果取 JWS 的 `environment`;谷歌有 `testPurchaseContext`(`fopType=TEST`)即为测试购买;微信取推送 `Env`;抖音没有沙箱。
- 生产环境里的沙盒或测试交易:**只对白名单账号入账**。白名单包括提交给 App Review 的演示账号和内部 QA 账号。入账时打 test 标记,不计营收、不进财务对账。白名单之外直接拒绝并告警。
- 配置:苹果、谷歌按"允许的环境集合"配置;生产与沙盒各一套端点和密钥。
- RTDN 的 `testNotification` 只回 200,不进业务。

**10.6 钻石入账失败**:

- 资产通道终局拒绝(非离线)时,trade 回执 `MANUAL`,进人工队列并告警。**绝不**因此给已付订单走退款。这是对 snapshot 分支现有行为(REJECTED → official_refund)的修改,见 §8.3.3。
- 因货币获取封禁被拒的,解封后人工重投。

## 11. 安全

- **不信客户端**:客户端只转交平台给的收据。任何"已付"判定都来自服务端向平台核验;入账字段一律取平台核验结果(苹果取 GTI 返回值,见 §5.3)。
- **绑定**:token 由服务端从订单号确定性派生,不含个人信息。谁提交收据都只能入账给 token 所属的玩家,截获别人的收据抢先提交拿不到任何东西。
- **密钥**:只经环境变量 / K8s Secret 注入,不进 git、不进日志(AGENTS §9)。启动时只校验"已配置且长度够",日志只写变量名。持有表见 §8.6。
- **通知入口**:
  - 苹果:按 §5.3 的清单验 `signedPayload`,嵌套的 `signedTransactionInfo` 单独验。
  - 谷歌 Pub/Sub push:验 OIDC Bearer,包括签名、`iss`、受众、服务账号邮箱、`email_verified`、`exp`。RTDN 消息体本身没有签名,只当线索用,一律回查 API。
  - 微信、抖音:按附录 A 的算法验签。
  - 验签失败**不落任何状态**、回非 2xx、计数。
- **mock 与管理面**:mock 只在 `Mode ∈ {dev, test}` 时可用;管理面用独立令牌、走 NetworkPolicy 隔离,人工动作双人审批并留审计(§5.4)。
- **日志与指标**:玩家 id 只进日志不进指标 label;交易号日志只写后 6 位;收据、token、session_key 原文不进日志。
- **限流**:`CreatePayIntent`、`SubmitStoreReceipt` 进 MessageLimiter;商城 `unbound` 单独限速。

## 12. 合规

- **国内上线收费必须先有版号**(2016 年 44 号文,现行有效)。没有版号前,国内包不开放充值入口。小游戏开通虚拟支付本身也要求版号和实名认证系统(附录 A)。
- **实名认证**:2021 年 14 号文要求所有用户实名注册登录、不能有游客模式。仓库目前零实现;接国家新闻出版署实名认证系统需要向属地出版局申请 bizID(该要求出自华为接入文档,未在署方原文核实)。
- **未成年人充值限额**(2019 年通知,现行有效),按**同一网络游戏企业**旗下所有游戏、所有账号合并计算:

  | 年龄 | 单次上限 | 每月累计上限 |
  |---|---|---|
  | 未满 8 岁 | 不得充值 | — |
  | 8 至 16 岁 | 50 元 | 200 元 |
  | 16 至 18 岁 | 100 元 | 400 元 |

  《未成年人网络保护条例》(2024-01-01 施行)第 44 条还要求限制单日累计消费,法规没给数字,**日上限由用户定**(§18.7)。
- **闸的实现**(`BuyerEligibility`):
  - **主体**:按实名身份(实名系统返回的 `pi`,或等价的身份哈希)统计,不按游戏账号。`payment_spend` 同时有月键和日键,北京时间。
  - **预占**:`CreatePayIntent` 在同一行锁下原子判断"已付 + 在途预占 + 本次 ≤ 上限",通过才预占。意图过期或取消时释放;到账时转为已付,按到账时刻计月。每个账号同时只允许 1 条在途充值意图。
  - **其它入账路径**:迟到付款复活、无原单入账、人工绑定也照常计入;超限时进人工、不自动发货(钱已付,由人工决定发货还是退款)。
  - **退款**:撤销回退累计,撤回重新计入。
  - **适用范围**:按玩家所在地区判断,不按渠道名。苹果看 storefront,国内渠道一律适用,非中国大陆店面不适用。
  - 生产环境开放国内地区充值而实名未接入时**拒启**。
- **平台规则**:
  - 苹果:中国区游戏币必须走 IAP(审核规则 3.1.1;允许外链付款的店面不含中国)。佣金:全球标准 30%、小型企业计划 15%;**中国区自 2026-03-15 起标准 25%、小型企业 12%**。
  - 谷歌:Billing 库自 2026-08-31 起须 ≥ 8(Unity IAP 5.4 内置 9.0.0)。费率:多数市场首 100 万美元 15%;美国、欧洲经济区、英国、澳大利亚、日本已换新结构(附录 A)。
  - 微信小游戏运营规范:禁止用户间直接或间接定向转移道具(三-2.5.1),所以**聚宝斋、玩家交易、拍卖在微信小游戏包里要关掉**;禁止用付费抽奖诱导消费(2.5.2)。

## 13. 客户端(Unity)

- 引入 `com.unity.purchasing` 5.4.x(当前 5.4.4)。v5 流程:
  1. `UnityIAPServices.StoreController()` → `Connect()` → `FetchProducts()` / `FetchPurchases()`;
  2. `OnPurchasePending` 时交给服务端;
  3. 服务端返回 `finish_transaction = true` 后调 `ConfirmPurchase()`。

  服务端在核实并落库后就返回 true,不等商城。原因:苹果对没有 finish 的交易不会在服务端重推,拖着不 finish 换不来可靠性,反而会出现"同一消耗型商品无法再次购买"。
- **iOS**:默认用 StoreKit 2,从 `Order.Info.Apple.jwsRepresentation` 取 JWS。`IAppleStoreExtendedService.SetAppAccountToken(Guid)` 是状态型 setter,每次拉起购买前用本次意图的 token 设一次,购买流程串行化;真机要核实 JWS 里确实带上了 token。
- **安卓**:用 `GooglePlayStoreExtendedService.SetObfuscatedAccountId/ProfileId` 设混淆号。谷歌商品**不开启"多数量购买"**。
- **谷歌消耗型**:客户端 `ConfirmPurchase` 也会触发 consume。服务端在落库后 consume,两侧谁先都可以,后到的一方把"已消耗"当成功处理(R2 真机核实)。
- **补交**:启动时 `FetchPurchases()`,把未结束的交易全部重交一次。
- **UI**:钻石包列表(`ListOfficialSkus` + 平台本地化价格)、付款中遮罩、到账提示。国内包没有实名前隐藏充值入口。微信小游戏在 `wx.checkIsSupportMidasPayment` 不支持时,连商城页面和按钮都不能出现。
- **国内安卓包不用 Unity IAP**:它只支持 App Store / Google Play,华为已移除,UDP 已于 2025-02-17 关停。国内按渠道或聚合 SDK 单独出包(R3)。

## 14. 配置与开关

```yaml
Mode: pro                     # go-zero 顶层 Mode;默认 pro;dev/test 才允许 mock;K8s staging/prod 由部署脚本强制 pro
Payment:
  Enabled: false              # 总闸,默认关;关闭时客户端协议一律回"暂未开放"
  Channels:
    mock:   { Enabled: false }                       # 只在 Mode ∈ {dev,test} 下可开
    apple:
      Enabled: false
      BundleId: ""
      AppAppleId: 0
      AllowedEnvironments: [Production, Sandbox]       # 生产也允许沙盒,但只对白名单入账(§10.5)
      Production: { IssuerIdEnv: PAYMENT_APPLE_ISSUER_ID, KeyIdEnv: PAYMENT_APPLE_KEY_ID, PrivateKeyEnv: PAYMENT_APPLE_P8 }
      Sandbox:    { IssuerIdEnv: PAYMENT_APPLE_ISSUER_ID, KeyIdEnv: PAYMENT_APPLE_KEY_ID, PrivateKeyEnv: PAYMENT_APPLE_P8 }
    google:
      Enabled: false
      PackageName: ""
      ServiceAccountEnv: PAYMENT_GOOGLE_SA_JSON
      PubSubAudience: ""
      PubSubServiceAccount: ""
    # wechat_minigame / douyin_minigame:由「微信抖音充值接入」会话按 production/sandbox 两组密钥变量名补
  TestAccountAllowlistEnv: PAYMENT_TEST_ACCOUNTS     # 沙盒 / 测试交易可入账的账号
  Skus:                       # sku_id → 各渠道商品 ID;启动时与商城核对
    - { SkuId: 1001, Apple: diamond_60, Google: diamond_60 }
  Mall: { BaseUrl: "http://127.0.0.1:18120", PaymentSubject: payment-adapter, PaymentSecretEnv: COMMERCE_PAYMENT_SECRET }
  Binding: { BindKeyEnv: PAYMENT_BIND_KEY, ObfKeyEnv: PAYMENT_OBF_KEY }
  AdminTokenEnv: PAYMENT_ADMIN_TOKEN
  Compliance: { RealNameProvider: none, DailyLimitMinor: 0 }   # 开放国内地区 + provider=none + Mode=pro → 拒启
```

## 15. 可观测性、告警与对账

- **指标**(低基数 label,只用 channel / result):
  - 核验请求数与耗时;
  - 通知数,按事件类型、验签结果分;
  - 推进结果;孤儿数;人工队列长度;
  - 谷歌未 consume 的最长时长;
  - 商城调用失败数;
  - 苹果通知最后成功处理时间;补漏拉到的新事件数。
- **告警**:
  1. 已核实超过 10 分钟仍未在商城入账;
  2. 商城已付超过 30 分钟仍未发货;
  3. 谷歌超过 48 小时未 consume;
  4. 通知验签失败率突增;
  5. 孤儿或人工队列非空;
  6. 白名单之外的测试 / 沙盒交易;
  7. 补漏拉到新事件(说明推送链路有丢失);
  8. 超过 N 小时没收到任何苹果通知;
  9. 商城 `unbound` 速率异常。
- **对账**:
  - 每日按渠道导出"平台交易 ↔ 商城回执 ↔ trade 发货 / 扣回"三方差异。
  - 苹果侧以 Notification History 为退款和到账的权威补漏来源;谷歌侧以 Voided Purchases 和 Orders API 为准。
  - 每月对平台财务报表:苹果 Sales and Trends、Play Console 财务报告。

## 16. 测试与验收

- **单测**:
  - 苹果验签。golden 向量由官方 Java 库生成,反例覆盖:篡改、错 bundleId、错环境、证书链断裂、"能链到 G3 但叶证书缺 OID"、`x5c` 不是 3 张、`alg` 不是 ES256、嵌套 JWS 被篡改、客户端 JWS 与 GTI 返回值不一致。
  - 谷歌:响应解析(含 `testPurchaseContext` 样例);"生产 + 非白名单测试购买 → 不入账";`quantity != 1` → 人工。
  - 交易号规范化;token 派生;合规闸边界(8 / 16 / 18 岁,单次 / 日 / 月,跨月迟到付款,并发建意图,撤销后再充值)。
  - `payment_store_txn` 状态机全部迁移。
- **集成**(真实 TiDB,mock 渠道):
  - 并发重交同一收据只入账一次;通知与客户端交收据同时到。
  - 落库后、调商城前崩溃;商城 200 后崩溃;商城 5xx 重试。
  - 已取消后付款复活;同一订单收到第二笔钱 → 无原单入账;攒满 5 张未付充值单后仍可下单。
  - 撤销先于入账 / 与入账并发;撤销落在 accept 与回执之间;**撤销落在玩家离线停放期间,上线后入账,断言最终产生扣回**。
  - 扣回时余额足够 / 部分不足 / 为 0;GM 欠款已存在或被冻结时补发;同一订单"退款 → 撤回 → 再退款"。
  - 充值腿被拒 → MANUAL,不退款;Tick 遇到坏待办跳过、不中断。
- **robot `recharge-smoke`**:登录 → 列 SKU → 下单 → mock 付款 → 等到账 → 校验钻石 +60 → mock 退款 → 校验扣回 → mock 撤回 → 校验补回。
- **沙盒**(需账号,R2):苹果沙盒账号 + TestFlight;谷歌 License testers 的"总是批准 / 总是拒绝 / 慢速"测试支付方式。抖音没有沙箱,mock 是唯一的无钱联调手段。

## 17. 分期

| 期 | 内容 | 需要用户做的 | 依赖 |
|---|---|---|---|
| R0 | 本文 + 两会话对齐 + 对抗评审 | 拍板 §18 | — |
| R1 | go/payment 骨架 + mock 渠道 + 商城 V3 + trade 发货 / 撤销裁决 / 扣回 / 补发 + C++ 充值扣回原语与 `TX_RECHARGE*` + 接入清单(§8.5)+ robot `recharge-smoke` | **批准 30+ 文件的改动范围与分批计划**(AGENTS §10.2)。每批的边界:proto 源与它的全部重生成物(go/proto、cpp/generated、robot/vendor、客户端生成物)同批,每批都能独立编译,并交 Codex 编译测试 | snapshot 分支合入 main;mall_game 的 K8s 部署 |
| R2 | 苹果 / 谷歌真实核验、通知与补漏 + 客户端 Unity IAP 与充值界面 | 办苹果开发者账号(688 元/年)、签付费协议、填银行税务、建商品、生成 .p8;办 Google Play 账号(25 美元)、建商品、建服务账号、配 Pub/Sub(附录 B) | R1 |
| R3 | 国内:实名认证 + 防沉迷限额 + 聚合 SDK 或逐家渠道 + 微信 / 抖音小游戏插件(另一会话) | 版号、实名系统 bizID、各渠道账号 | R1 |
| R4 | Java 版服务端实现对齐(game-mall V3 等) | — | R1 |

## 18. 待决问题

1. **snapshot 分支归属与合入时间**(阻塞 R1)。
2. 钻石包档位与数量(§4 默认表)是否照用。
3. 苹果 `CONSUMPTION_REQUEST` 是否回传消耗信息。回传通常能降低恶意退款的成功率,但要先征得用户同意分享数据,并写进隐私政策;回传须在 12 小时内。
4. 退款扣回之外是否附加处罚(封号 / 禁充值)。
5. Java 版排期,以及 R1 客户端契约变更与 Java 同批的做法(§19)。
6. 生产环境测试交易白名单的管理方式:谁加、加哪些账号。
7. 未成年人单日消费上限定多少。

## 19. 双版本(AGENTS §12)

两版共享的是客户端契约:`proto/payment/payment.proto` 的客户端 service、`payment_error` tip 段、`CommerceOrderState` 的新值(REVERSED)。

- 这些按 AGENTS §12 **应与 Java 版同批落地**。Java 侧至少要有协议,加上"暂未开放"的实现。
- 做不到同批时(例如本机没有 Java 仓),R1 在 `PARITY.md` 与 `PROGRESS.md` 登记"待做 + 原因",并交用户确认这次偏离。
- Java 侧的服务端实现(苹果有官方 Java 库 `com.apple.itunes.storekit:app-store-server-library`,谷歌有官方 `google-api-services-androidpublisher`)和 game-mall V3 对齐放到 R4。

## 20. 修订记录

- **v1(2026-10-19)**:初稿。
- **v2(2026-10-19)**:按对抗评审(5 角度 × 评审 + 核实)与「微信抖音充值接入」会话的意见修订:
  - 迟到付款按 snapshot 语义改为"商城复活订单"(原稿以为会回 409);
  - 扣回从"GM 收件箱 attach_debt(只记欠款不扣余额)"改为 scene 原子原语"先扣余额、差额记充值欠款";
  - 撤销裁决从商城移到 trade(原稿"先到者赢"在非破坏性 poll 下会漏扣);
  - tx_type 下沉到腿;充值腿被拒不退款;
  - 商城改动面列全:发放物二选一、取消、未付上限、只读接口、角色表、错误码;
  - payment_store_txn 状态机与推进任务;finish_transaction 改为落库即结束;
  - 苹果验签清单与以 GTI 为准;Notification History 补漏;
  - 生产环境接受白名单测试交易;谷歌测试购买识别;
  - 合规按实名身份、预占额度、日上限;
  - 数量恒 1;确定性 token;
  - Mode 用 go-zero 顶层;管理面鉴权;接入清单;30+ 文件须用户批准;双版本口径;
  - 小游戏接口名更正(`wx.requestMidasPaymentGameItem`)与平台事实。

## 附录 A 平台事实(2026-10-19 核实,带官方链接)

**苹果**

- 核验:`GET https://api.storekit.apple.com/inApps/v1/transactions/{id}`(沙盒 `api.storekit-sandbox.apple.com`;2026-05-05 起推荐此域名),返回 `signedTransactionInfo`。[Get Transaction Info](https://developer.apple.com/documentation/appstoreserverapi/get-transaction-info)
- 鉴权:ES256 JWT。头 `kid` = Key ID;载荷 `iss` = Issuer ID、`aud` = `appstoreconnect-v1`、`bid` = Bundle ID;有效期 ≤ 60 分钟;用 `.p8` 私钥签。[生成 JWT](https://developer.apple.com/documentation/appstoreserverapi/generating-json-web-tokens-for-api-requests)
- 验签规格对齐官方库:`AppleExtensionCertPathChecker`(两个 OID)与 `ChainVerifier`(链长 3)。[Java 库](https://github.com/apple/app-store-server-library-java)
- `verifyReceipt` 已废弃。官方库只有 Swift / Java / Python / Node 四种,没有 Go;Java 版坐标 `com.apple.itunes.storekit:app-store-server-library:5.2.0`。
- `appAccountToken` 必须是 UUID;服务端可用 Set App Account Token 补设(2025-06 新增)。[文档](https://developer.apple.com/documentation/appstoreserverapi/appaccounttoken)
- 通知 V2:`ONE_TIME_CHARGE`、`REFUND`、`REFUND_DECLINED`、`REFUND_REVERSED`、`CONSUMPTION_REQUEST`(12 小时内回 `PUT /inApps/v2/transactions/consumption/{id}`)。投递失败时生产重试 5 次(1/12/24/48/72 小时),沙盒只发一次;漏掉的用 Get Notification History 补。[类型](https://developer.apple.com/documentation/appstoreservernotifications/notificationtype) [重试与补漏](https://developer.apple.com/documentation/appstoreservernotifications/responding-to-app-store-server-notifications)
- 审核期间:审核员用正式包、走沙盒付款,生产服务器需能核验沙盒交易。[Validating receipts](https://developer.apple.com/documentation/storekit/validating-receipts-with-the-app-store)
- 中国区佣金 2026-03-15 起 25% / 12%。[公告](https://developer.apple.com/news/?id=dadukodv)

**谷歌**

- Billing 库最新 9.1.0;2026-08-31 起新应用与更新须 ≥ 8。[发布记录](https://developer.android.com/google/play/billing/release-notes)
- 一次性商品核验推荐 `purchases.productsv2.getproductpurchasev2`。它**不返回价格**,有 `testPurchaseContext`(只对测试购买设置,`fopType=TEST`)和 `productOfferDetails.quantity`。实收金额用 Orders API(`orders.get`)取;退款用 `orders.refund`。[productsv2](https://developers.google.com/android-publisher/api-ref/rest/v3/purchases.productsv2)
- 消耗型用 `purchases.products.consume`(同时算确认);3 天内不确认自动退款,从"待付款"变"已购买"时起算。[一次性商品](https://developer.android.com/google/play/billing/lifecycle/one-time)
- 服务账号:Cloud 项目启用 API → 建服务账号 → Play Console"用户和权限"邀请,授"查看财务数据""管理订单和订阅";**已不需要关联 Cloud 项目**。[文档](https://developers.google.com/android-publisher/getting_started)
- 混淆号各 ≤ 64 字符,禁止明文个人信息。
- RTDN:Pub/Sub 主题授权 `google-play-developer-notifications@system.gserviceaccount.com`;后台须选"所有订阅和一次性商品事件";退款经 `voidedPurchaseNotification`。Voided Purchases API 覆盖近 30 天,每天 6000 次、每 30 秒 30 次,`includeQuantityBasedPartialRefund` 默认不含按数量部分退款。[RTDN](https://developer.android.com/google/play/billing/rtdn-reference) [Voided](https://developers.google.com/android-publisher/voided-purchases)
- 费用:开户 25 美元;多数市场首 100 万美元 15%;美、欧、英、澳、日已换新结构(以官方页为准)。[费率](https://support.google.com/googleplay/android-developer/answer/112622)

**Unity IAP**:最新 5.4.4(2026-10-02);iOS 默认 StoreKit 2;Android 内置 Billing 9.0.0;`IAppleStoreExtendedService.SetAppAccountToken(Guid)` 在 5.4 API 参考中;只支持 App Store / Google Play。[API](https://docs.unity3d.com/Packages/com.unity.purchasing@5.4/api/UnityEngine.Purchasing.IAppleStoreExtendedService.html) [支持的商店](https://support.unity.com/hc/en-us/articles/31133413719060-In-App-Purchasing-IAP-supported-stores)

**微信小游戏**(会话「微信抖音充值接入」核实,2026-10-19)

- **道具直购(虚拟支付 2.0)**:
  - 前端 `wx.requestMidasPaymentGameItem`,服务端接口在 `/wxa/game/*` 下。
  - `signData` 是 JSON 串,字段:`mode="goods"`、`offerId`、`buyQuantity`、`env`、`currencyType=CNY`、`platform=android`、`zoneId`、`productId`、`goodsPrice`(分,须与 MP 后台道具价一致)、`outTradeNo`、`attach`。
  - `outTradeNo` ≤ 32 字符,字符集 `[0-9A-Za-z_-|*@]`,不能以下划线开头,只能用一次。
  - `signData` 必须与前端逐字节一致。
- **签名**:
  - `paySig = hex(HMAC-SHA256(AppKey[env], "requestMidasPaymentGameItem&" + signData))`;
  - `signature = hex(HMAC-SHA256(session_key, signData))`,session_key 按 utf-8 串直接当 key(被签对象由官方链接推断)。
  - AppKey 按 env 分两把:0 = 正式,1 = 沙箱。正式版只能用正式环境;iOS 不支持沙箱。
- **到账推送**:
  - 事件 `minigame_game_pay_goods_deliver_notify`;推送体 `{Event, MiniGame:{Payload, PayEventSig}}`;安全模式先解密再验签。
  - `pay_event_sig = hex(HMAC-SHA256(AppKey[env], Event + "&" + Payload))`。
  - Payload 字段:`OpenId / Env / OutTradeNo / GoodsInfo{ProductId, Quantity, ZoneId, OrigPrice, ActualPrice, Attach, OrderSource} / PayChannel`。`WeChatPayInfo` 不保证返回。
  - 回包明文:成功 `{"ErrCode":0,"ErrMsg":"Success"}`,失败 `{"ErrCode":99999,…}`。
  - 3 秒超时;URL 只能用 80/443;共推 15 次,约 24 小时。
  - `IsMock=true` 要回成功、不产生事件。
- **退款**:`minigame_pay_refund_succ_notify`(12 小时内推 13 次),验签同上;没有开发者主动退款接口。
- **查单**:`pay_v2.queryOrder`,uri 未核实;服务端签名 `pay_sig = hex(HMAC-SHA256(AppKey[env], uri + "&" + post_body))`。可能还需要用户 session_key 签名,所以后台查单可能做不了。
- **iOS**:2026-01 起通过苹果 Mini Apps Partner Program 开放。要求:
  - 签约"iOS 资金概况";
  - iOS 15+、基础库 3.10.3+、微信 8.0.68+;
  - 先调 `wx.checkIsSupportMidasPayment`,不支持时(返回 701001)连商城入口都不能出现。
- **分成**:开发者 iOS 70%(2026-12-31 前限时约 88%)、安卓 60%。
- **开通条件**:企业或个体工商户的已认证小游戏;已接入中宣部实名认证系统;提交版号、软著自审自查报告。

**抖音小游戏**(同上会话核实)

- **安卓**:`tt.requestGamePayment{mode:"game", env:0, currencyType:"CNY", platform:"android", goodType:2, orderAmount(分), goodName(≤10 字), customId, extraInfo}`。
  - 要求基础库 3.47.0+、抖音 32.0.0+,先 `tt.canIUse('requestGamePayment.object.goodType')`。
  - 没有服务端签名,金额可被前端篡改。
- **iOS**:`tt.openAwemeCustomerService`,`currencyType:"DIAMOND"`,1 元 = 10 钻,iOS 用户需多付约 30%。抖音小游戏没有接苹果 IAP 的文档。
- **回调**:
  - 配置时平台发 GET 校验:`token/timestamp/nonce/msg` 按字典序排序后拼接,做 SHA1,取小写 hex 与 signature 比对,通过后回 `echostr`。
  - 之后只对支付成功发 POST,body 是 JSON `{timestamp, nonce, msg, signature}`,验签同上。官方 Go 示例的 `make([]string,4)` 会多出空串,别照抄。
  - `msg` 字段:`appid / cp_orderno(=customId) / cp_extra / order_no_channel / amount_cent / amount_coin / currency`。
  - 回 HTTP 200 即可,否则重试 16 次,约 4 小时 45 分。
- **查单**:`GET https://developer.toutiao.com/api/apps/game/payment/queryPayState?access_token=&orderno=<customId>`,`unsuccess` 不是终局失败。
- **没有沙箱**,真机调试会真扣钱;没有开发者主动退款接口。
- **开通**:必须有版号,并在 `wlc.nppa.gov.cn` 把抖音主体加为渠道,勾选实名上报与数据上报。分成一般是开发者 60%。

**国内其它**

- 华为 IAP 核验:`POST {rootUrl}/applications/purchases/tokens/verify`,`Authorization: Basic base64("APPAT:"+access token)`,返回数据用 IAP RSA 公钥验签;中国站 rootUrl 未核实。[文档](https://developer.huawei.com/consumer/en/doc/development/HMSCore-References/api-order-verify-purchase-token-0000001050746113)
- 微信支付 APIv3:
  - APP 下单 `POST /v3/pay/transactions/app`(分,prepay_id 有效 2 小时);
  - 回调验签串"时间戳\n随机串\n报文\n",资源用 AEAD_AES_256_GCM 解密;
  - 5 秒内回 200/204;失败最多重试 15 次,约 24 小时;注意 `WECHATPAY/SIGNTEST/` 探测包。[回调](https://pay.weixin.qq.com/doc/v3/merchant/4013070368)
- 支付宝异步通知用 `rsaCheckV1` 验签;回包与重试规则未核实。
- 聚合 SDK:QuickSDK 仍在更新;易接、AnySDK 未核实。渠道分成常见约 5:5,未核实。
- 法规:[版号(2016 年 44 号文)](https://www.nppa.gov.cn/xxfb/zcfg/gfxwj/201606/t20160602_4492.html)、[实名(2021 年 14 号文)](https://www.gov.cn/zhengce/zhengceku/2021-09/01/content_5634661.htm)、[充值限额(2019 年通知)](https://www.gov.cn/zhengce/2019-11/05/content_5448848.htm)。2023-12 征求意见稿与 2026-09-18 网信办征求意见稿均未生效。

## 附录 B 用户需要亲自办的事(Claude 不能代办)

1. **苹果**:
   - 加入 Apple Developer Program(国内 688 元/年);
   - 在 App Store Connect 签 Paid Applications Agreement,填银行与税务;
   - 建 App 与 6 个消耗型商品;
   - 生成 App Store Server API 的 `.p8` 密钥,记下 Issuer ID、Key ID;
   - 建沙盒测试账号;
   - 配置 App Store Server Notifications V2 的生产与沙盒地址;
   - 提审时在审核备注里提供演示账号,并把它加进测试白名单。
2. **谷歌**:
   - 注册 Google Play Console(25 美元),填付款资料;
   - 建 App 与 6 个一次性商品,**不开启多数量购买**;
   - 在 Google Cloud 建服务账号,并在 Play Console 授权;
   - 建 Pub/Sub 主题与推送订阅;
   - 登记 License testers。
3. **国内(R3)**:版号;实名系统 bizID;各渠道或聚合 SDK 账号;微信小游戏 / 抖音小游戏的虚拟支付开通;微信支付 / 支付宝商户号(若做官网包)。
4. 拿到的密钥**不要发到聊天里**。直接写进本机环境变量或 K8s Secret,告诉 Claude 变量名即可。
