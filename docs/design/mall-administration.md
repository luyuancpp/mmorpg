# 商城后台的受限经济命令

状态：2026-10-21 实施中，尚未编译或运行。本设计沿用 D-17 的商城订单权威与现有 PMS/OMS，不新增游戏钱包。

## 职责与接口

后台目录模块负责 SKU 条款、发布状态和库存调整；已有订单的金额、游戏映射、付款及交付记录不可被后台改写。管理查询返回包含未发布 SKU 的有界主键页，以及只读原订单视图。经济命令只接受创建 SKU、修改新订单条款和按差额调整库存，不提供任意 SQL、REPLACE、直接置已付或置已发货接口。

内部接口使用独立的 admin 角色、主体和至少 32 字节的签名密钥，不能复用 gateway/payment/delivery 密钥。未配置管理密钥时管理接口拒绝，但现有商城入口照常工作；控制器再次核角色。浏览器不持有商城签名密钥，管理界面通过既有运维鉴权代理这些固定命令。

## 提交点与幂等

每项写命令携带不透明 requestId；同一已认证管理主体与请求号只提交一次。请求摘要覆盖命令种类与完整类型化参数，同请求换条款拒绝。成功响应快照、原请求摘要和业务写入在同一 TiDB 事务提交；提交后回包丢失重放返回原响应，不能再调整一次库存。

事务按管理请求主键、SKU 主键的固定顺序取锁。新增 catalog_revision 只用于管理条款的乐观并发控制；普通下单预占、付款扣库存不增加此修订，但管理写入必须在持 SKU 行锁后使用当时真实 stock/lock_stock 复核。修改条款保留 stock/lock_stock；库存调整使用差额且调整后 stock 不小于 lock_stock，不允许绝对库存覆盖。修订号溢出拒绝。

SKU 的新价格、标题及发物映射只影响未来订单。已有订单继续读取 oms_order 的冻结价格与 commerce_order_grant 的冻结映射。成功命令日志只追加，不向普通玩家开放；认证 nonce 仍按原协议独立消耗。失败或未知提交不伪装成功，重试使用原请求号。

## 存储与部署

第三方商城的 SQL 迁移例外继续适用。新安装 schema 与 V3__catalog_admin.sql 同批增加 catalog_revision 及 commerce_admin_command；启动形状守卫核对必要列、二进制请求键、唯一约束和金额类型。V3 只给既有 V2 库升级，不在已含此结构的新库重复运行。充值稿原先预留的 V3 尚未实现，实际后续充值迁移必须使用新的版本号，不复用这个版本。

两版独立部署、独立存储，管理内部接口形状同步；真实渠道按负责人本轮决定留待以后，本批管理面不能绕过该决定开放渠道或凭空写支付事实。

## 运维入口与页面

两版内部提供 `POST /internal/admin/skus/{list,create,update,stock}` 与 `/internal/admin/order-view`；后者复用原订单只读视图，admin不具有payment权限。主仓gateway_node代理 `POST /admin/mall/` 下同五条固定路径，复用X-Admin-Key；新商城代理额外要求配置令牌至少32字节，方法内再次鉴权，context/servlet前缀或forward不能绕过。Java版管理面在game-data，复用其X-Game-Admin-Token、X-Game-Operator与既有审计，方法内核认证标记，不把运维入口挪到game-gateway。

`GET /mall-admin` 及同级JS/CSS仅提供公开页面壳。令牌只在当前密码输入框内，清除或离开页面时移除，不写浏览器存储；商城管理HMAC密钥不渲染。页面支持有界目录查询、原订单查询、条款与库存差额命令。64位ID／修订号用十进制字符串提交，响应按原文本呈现，避免JavaScript浮点数改写数值；标题和返回数据只写文本节点。页面未做分类运营、促销或任意订单状态编辑。

代理地址来自受信服务端配置，只允许五条固定命令，保留原JSON字节与客户端requestId。请求体最多16KiB、响应体最多256KiB；严格解析整个JSON文档，尾随垃圾或第二个根对象拒绝。HMAC绑定完整上游部署前缀，每次使用新nonce；连接预算3秒、完整响应future预算8秒，超时取消并保留未知提交语义，不自动重试写命令、不跟随重定向。页面超时保留原请求号，提示查询／原请求重放。

主仓配置 `GATEWAY_MALL_ADMIN_URL`、`COMMERCE_ADMIN_SECRET`、`COMMERCE_ADMIN_SUBJECT`（缺省mall-admin），并通过既有admin.api-key配置足够强的运维令牌。Java配置 `GAME_DATA_MALL_ADMIN_URL`、`GAME_MALL_ADMIN_SECRET`、`GAME_MALL_ADMIN_SUBJECT`，使用原GAME_ADMIN_TOKEN；未配置代理地址时只关闭代理。所有配置只写环境变量名，不把实际秘密写入跟踪文件。

## 验收

必须验证同请求并发和提交后重放只改一次、换载荷拒绝、两个修订竞争者仅一个提交、当前预占不能被价格或库存命令抹掉、已有订单金额与映射不变、库存下界与整数溢出、越权及未知路由拒绝。H2 与 HTTP 用例只验证相应接缝，另跑真实 TiDB，不能冒充完整后台或真实渠道验收。

本批另覆盖缺发物映射的历史SKU可查询／修复、修复前库存命令409且不留journal；新响应与重放都核合法完整条款，坏响应日志（包括null、空对象、尾随垃圾与第二根JSON）503，不接管重做。V2→V3回归执行同一升级SQL并核旧订单／预占保留。代理测试使用本例随机loopback服务，不连接他人服务。页面已执行JS语法检查与本轮自有静态预览的浏览器视觉检查：三列中文控件、请求号和响应区可读；静态预览明确返回503未连接后端，没有输入凭据或发起实际经济命令，结束后已关闭预览进程与监听。证据见`build/verification/mall-admin-visual-20261021T050444326Z/preview-result.json`；真实鉴权代理、商城写入和联机页面仍待执行验收。

### 当前本机验收接线（2026-10-21）

主仓网关使用JDK23，商城使用JDK21；页面是classpath静态资源，没有npm工程。网关的管理代理仍只有上述五条命令，不能代替payment角色发付款或交付回执。`start_game.ps1`当前没有自动拉起商城，验收由root单独批准当前商城JAR与子进程环境；不为冒烟打开全局默认开关。

商城真TiDB夹具显式接受数字loopback合法非零端口，且不得带库名、凭据或查询参数；本任务root只使用已核归属的`127.0.0.1:24000`。建随机库前先核TiDB及强读会话；四物理连接和弱读／历史快照拒绝断言保留。普通H2与loopback HTTP测试仍不等同真库或游戏到账证据。

`robot`的`mall-arrival-smoke`已改用`POST /internal/mock/pay`，正文只有原`orderId`和由该订单派生的稳定`requestId`。商城读取冻结金额并先提交mock提供方原件，robot不再向旧`/internal/payments`提交`mock-dev`五元组，也不增加渠道白名单。先验证额外金额字段400拒绝、原单仍待付款，再以两个原字段付款并重放，核原买家、金额、币种、SKU、份数；后续原到账、三拍不重发、重登存盘及遗留原单清理保持。`COMMERCE_RUN_MODE=dev`与`COMMERCE_MOCK_PAYMENT_ENABLED=true`必须同时显式设置，`COMMERCE_ALLOWED_PAYMENT_CHANNELS`为空。未知付款提交不换请求号。

本段是源码接线与待执行口径。本批未运行Maven、Go测试、mock接口、商城进程或游戏到账链；完整本地命令、环境名和各层通过标准置于`build/verification/current-completion-20261021/mall-admin-mock-current-acceptance-20261021/`，不把已有静态页面预览作为真实管理链路通过。
