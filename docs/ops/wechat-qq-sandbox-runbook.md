# WeChat / QQ Sandbox 接入清单 (todo #223 / U)

**Date:** 2026-05-10
**目的:** 让接入第三方 OAuth 的人 30 分钟内完成"申请 sandbox AppId → 配置 login.yaml → 跑通端到端"全流程,不需要再翻散落的代码。
**前置:** [auth-provider-framework.md](../design/auth-provider-framework.md), [third-party-login-end-to-end-design.md](../design/third-party-login-end-to-end-design.md), [release-checklist.md](./release-checklist.md)

---

## A. 沙箱凭证申请

### A.1 WeChat Open Platform

**入口:** <https://open.weixin.qq.com/>

| 字段 | 怎么拿 |
|---|---|
| AppId | 控制台 → 管理中心 → 移动应用 / 网站应用 → 创建后得到 `wx1234567890abcdef` 形态 |
| AppSecret | 同页面"查看"按钮(只显示一次,丢了得重置) |
| OAuth 域名白名单 | 必须把 staging gateway 的 host 加进"授权回调域";本地测试用 `127.0.0.1:8081` 不行,**用 ngrok / Cloudflare Tunnel** 套一层 https |
| UnionId | 在 Open Platform(不是公众平台)绑同一主体下的多个 App 后才会下发,**这是我们 `wx_<unionid>` 账号映射的依据** |

**我们用到的接口**(`go/login/internal/logic/pkg/auth/providers.go:114`):
```
GET https://api.weixin.qq.com/sns/oauth2/access_token
    ?appid=<AppId>&secret=<AppSecret>&code=<from-SDK>&grant_type=authorization_code
返回: { access_token, openid, unionid?, expires_in, ... }
```

> **沙箱限制:** 每天 ~5000 次 `code → access_token` 调用上限;够测试,不够压测。

### A.2 QQ Connect

**入口:** <https://connect.qq.com/>

| 字段 | 怎么拿 |
|---|---|
| AppId | "开发者中心 → 应用列表"得到 `10xxxxxxxx` 数字 |
| AppKey | 同页(我们当前**没用**到 AppKey;客户端 SDK 直接拿 access_token) |
| 回调域 | 同 WeChat,把 staging gateway 域名加白 |

**我们用到的接口**(`providers.go:185`):
```
GET https://graph.qq.com/oauth2.0/me?access_token=<from-SDK>&unionid=1&fmt=json
返回(JSONP 包裹时也能解):
   { client_id, openid, unionid? }
```

> **客户端拿 access_token,不是 OAuth code。** 不要把 WeChat 的协议套到 QQ 上。

---

## B. 把凭证灌进 login.yaml

`go/login/etc/login.yaml` 已有占位段(注释),取消注释、替换 env 变量:

```yaml
AuthProviders:
  SaToken:
    TokenName: satoken
    LoginType: login
    Redis:
      Host: 127.0.0.1:6379
      Password: ""
      DB: 0
      DefaultTTL: 24h
      DialTimeout: 3s
      ReadTimeout: 3s
      WriteTimeout: 3s

  # 接入后取消注释。staging 用 sandbox 凭证,prod 用正式凭证。
  WeChat:
    AppId: "${WECHAT_APP_ID}"           # K8s Secret 注入,不要硬编码
    AppSecret: "${WECHAT_APP_SECRET}"

  QQ:
    AppId: "${QQ_APP_ID}"
    AppKey: "${QQ_APP_KEY}"             # 当前 provider 没用,留 placeholder
```

> 注入方式:K8s `Secret` → `envFrom` → go-zero `Config` 自动展开 `${VAR}`(go-zero 1.10 默认支持)。本地测试 `export WECHAT_APP_ID=...; export WECHAT_APP_SECRET=...; ./login.exe -f login.yaml`。

启动后日志应有:
```
Auth provider registered: wechat
Auth provider registered: qq
```
没出现就是 `cfg.WeChat == nil`,检查 yaml 缩进或 env 变量。

---

## C. 端到端联调步骤

### C.1 单元 / 集成层(本机就能跑)

已经 OK,无需新跑(在 main 上一直绿):

```bash
# go-zero 这层 mock-server e2e:
cd go/login && go test -count=1 -run TestWeChatProvider ./internal/logic/pkg/auth/

# Java Gateway 这层 wire-format e2e:
cd java/gateway_node && mvn -Dtest='LoginEndpointIntegrationTest' test
# 命中 loginEndpoint_propagatesAuthTypeAndAuthToken_wechat / _qq
```

### C.2 真 Gateway → 真 login → 真微信 / 腾讯

**前置**: §A.1 / §A.2 已拿到 sandbox 凭证;§B 已配置;login + Java Gateway 已起。

```bash
# WeChat 用真 SDK 拿到的 code(从手机扫码 / Web 授权回调)
CODE=<the-5-min-oauth-code-from-sdk>

curl -v -X POST http://${GATEWAY_HOST}:8081/api/login \
  -H 'Content-Type: application/json' \
  -d "{
    \"zone_id\": 1,
    \"auth_type\": \"wechat\",
    \"auth_token\": \"${CODE}\",
    \"device_id\": \"sandbox-test-1\"
  }"
```

**期望返回**:
```json
{
  "code": 0,
  "players": [],
  "access_token": "<32 random bytes hex>",
  "refresh_token": "<32 random bytes hex>",
  "access_token_expire": <now + 7200>,
  "refresh_token_expire": <now + 2592000>
}
```

**典型失败 + 排查**:

| 现象 | 原因 | 解 |
|---|---|---|
| `code:401 message:"wechat: empty code"` | provider 收到空 token | 客户端没填 `auth_token`,检查 SDK 集成 |
| `code:401 message:"wechat: errcode=40029"` | code 已用过 / 失效 | 重新走 OAuth,**code 一次性 + 5 min 过期** |
| `code:401 message:"wechat: errcode=40125"` | AppSecret 不对 | yaml 里值错了 / env 没注入 |
| `code:500 message:"login_unavailable"` | login.rpc 没起 / 网络不通 | `gh:auth status`、`docker ps`、`netstat -an grep 50000` |
| `code:0 但 access_token 空` | login 没注册 wechat provider | 看 login 启动日志 `Auth provider registered: wechat` 是否出现 |

### C.3 验证账号映射

第一次成功后:

```bash
# Redis 应有
docker exec redis redis-cli KEYS 'account_data:wx_*'
# 应该看到 account_data:wx_<unionid> 或 account_data:wx_<openid>

# UserAccounts 应有(MySQL)
docker exec mysql mysql -u root -p<pw> mmorpg \
  -e "SELECT account, account_type FROM UserAccounts WHERE account LIKE 'wx_%' LIMIT 5;"
```

`account` 字段以 `wx_` / `qq_` 前缀开头 = provider 框架按设计工作。

### C.4 第二次同 unionid 登录(账号复用)

用同一个微信号再走一遍 OAuth(**新 code**),期望:
- 不创建新账号
- 返回**同一个 player_id**(玩家继承上次进度)
- access_token 是新的(每次 issue 新 token)

如果返回的 player_id 不一样,说明 unionid 没取到、退而求其次用 openid → openid 跨 App 不一致 → 看 §A.1 的 UnionId 行,绑同一主体下多个 App。

---

## D. 容易踩的 4 个坑(以本仓库实际经验)

### D.1 OAuth 域名白名单严格匹配

WeChat Open Platform 的"授权回调域"**只接受顶级域 + 二级域**(`example.com`、`*.example.com`),**不接受路径、不接受端口、不接受 IP**。`127.0.0.1:8081` 会被拒。

**解法**:本地测试套 ngrok:
```bash
ngrok http 8081
# 把 ngrok 给的 https://xxx.ngrok-free.app 顶级域填到回调域
```

### D.2 staging vs prod 凭证混用

凭证一旦混(staging code 拿到 prod login,反之亦然)→ `errcode=40029` 一直循环。yaml `${VAR}` 注入 + K8s namespace 隔离,不要用同一个 Secret 跨 namespace。

### D.3 AppSecret 进 git

```bash
# pre-commit hook(可加到 .git/hooks):
git diff --cached | grep -E '(AppSecret|AppKey)\s*[:=]\s*["\x27][^"$\x27]' && {
  echo "ERROR: literal AppSecret/AppKey detected; use \${VAR} placeholder"
  exit 1
}
```

### D.4 unionid 没拿到

控制台没把当前 App 加到"主体"分组 → 微信 API 不返回 unionid,只返回 openid。我们 provider 会 fallback 到 `wx_<openid>`,但**openid 跨 App 不一致**——同一玩家在两个 App 里会变成两个账号,无法迁移。

**解法**:Open Platform 控制台 → 主体管理 → 把所有 App 加进同一主体 → 等 ~10 min 生效后重新走 OAuth → 验证返回里 `unionid` 非空。

---

## E. 上 prod 前的 checklist

- [ ] `WECHAT_APP_SECRET` / `QQ_APP_KEY` 在 K8s Secret 里(不在 yaml literal)
- [ ] OAuth 回调域已加 prod gateway 顶级域(staging 域不要复用)
- [ ] login 启动日志确认 `Auth provider registered: wechat / qq`
- [ ] sandbox 至少跑通过一次 §C.2 / §C.3(否则就是上 prod 才发现集成不通)
- [ ] 监控有 `wechat: errcode=*` 计数(沿用 [grafana-login-path-deprecation.json](./grafana-login-path-deprecation.json) 的 panel 模式扩展)
- [ ] 流量按 zone 灰度:先开放给一两个 zone,观察 24h `account_data:wx_*` 增长是否合理(没大量重复账号)

---

## F. 我做这件事的进度

完成度 ⚠️ — 本机 dev 没有真 sandbox 凭证,所以 §C.2 / §C.3 由有凭证的同事跑。我做完的部分:

- ✅ §B 配置示例已注释在 `go/login/etc/login.yaml` 里
- ✅ §C.1 mock-server / wire-format 单元测试在 main(LoginEndpointIntegrationTest 7/7 含 wechat / qq)
- ✅ §D 坑位踩过的对照(`providers_test.go` 验证过 errcode 解析)
- ✅ **`Endpoint` 配置字段** — `WeChatProvider` / `QQProvider` 都支持指向自建 sandbox(详见 §G)
- ✅ **本地 sandbox mock server**(`go/login/cmd/sandbox_mock`)— 在没真凭证时也能跑 wechat/qq 端到端
- ⚠️ §C.2 / §C.3 真沙箱跑数 — 留位,需要拿到真凭证的人执行后填回本文 §C.2 末尾的 fixtures

---

## G. 没真凭证的端到端验证(用 sandbox_mock)

如果暂时拿不到 WeChat Open Platform / QQ Connect 的真 AppId/AppSecret(申请审批 / 网络出口 / 公司流程都可能延迟),不要原地等——本仓库自带一个**字节级模拟**这两个 API 的 mock 服务,可以验证整条 `Client → Gateway → gRPC login → Provider → HTTP API → 账号映射 → token` 链路的正确性,只缺"流量真到了腾讯/微信服务器"这最后一公里。

### G.1 起 mock server

```bash
cd /f/work/mmorpg
go build -o bin/sandbox-mock ./go/login/cmd/sandbox_mock
./bin/sandbox-mock -addr :18090 &
# 默认 :18090,可改 -addr :19091 之类的避端口冲突
```

mock 暴露的 endpoint(响应字节级对齐真 API):

| 路径 | 模拟 | 特殊 code |
|---|---|---|
| `GET /sns/oauth2/access_token` | `api.weixin.qq.com` | `code` 以 `U` 开头→返回 unionid;`code=EXPIRED`→errcode 40029 |
| `GET /oauth2.0/me` | `graph.qq.com` | `access_token=INVALID`→error 100007 |
| `GET /healthz` | — | 返回 `ok`,用于 liveness |

**deterministic 哈希**:同一 `code`(WeChat)/ `access_token`(QQ)每次返回**完全相同**的 openid / unionid。这是 §C.4 账号复用验证的前提——不会因为 mock 重启就把"老账号"洗成新账号。

### G.2 让 login 走 mock 而非真 API

`go/login/etc/login.yaml`:

```yaml
AuthProviders:
  WeChat:
    AppId: "wx_sandbox_test"
    AppSecret: "any_string_ok_here"      # mock 不校验
    Endpoint: "http://127.0.0.1:18090"   # ← 关键:加这一行就走 mock
  QQ:
    AppId: "100000001"
    AppKey: "ignored"
    Endpoint: "http://127.0.0.1:18090"   # 同一个 mock 服务两条 path 都收
```

重启 `login`,期望日志:
```
Auth provider registered: wechat (endpoint override: http://127.0.0.1:18090)
Auth provider registered: qq (endpoint override: http://127.0.0.1:18090)
```

### G.3 端到端 curl

**WeChat 首次登录 — 期望拿 unionid + 进游戏**:

```bash
curl -s -X POST http://127.0.0.1:8081/api/login \
  -H 'Content-Type: application/json' \
  -d '{"zone_id":1,"auth_type":"wechat","auth_token":"Utest1","device_id":"sandbox-1"}'
```

期望返回 `code:0` + access/refresh token。检查 Redis:
```bash
docker exec redis redis-cli KEYS 'account_data:wx_unionid_*'
# 应看到 account_data:wx_unionid_124806e0b96c
```

**第二次同 code(账号复用)** — 期望同一 player_id:
```bash
curl -s -X POST http://127.0.0.1:8081/api/login -H 'Content-Type: application/json' \
  -d '{"zone_id":1,"auth_type":"wechat","auth_token":"Utest1","device_id":"sandbox-1"}'
# 返回里的 player_id 应该和上一次一致
```

**WeChat code 不以 U 开头(无 unionid)— 期望账号回落 openid**:
```bash
curl -s -X POST http://127.0.0.1:8081/api/login -H 'Content-Type: application/json' \
  -d '{"zone_id":1,"auth_type":"wechat","auth_token":"plain_code","device_id":"sandbox-1"}'
docker exec redis redis-cli KEYS 'account_data:wx_openid_*'
```

**WeChat code 过期**:
```bash
curl -s -X POST http://127.0.0.1:8081/api/login -H 'Content-Type: application/json' \
  -d '{"zone_id":1,"auth_type":"wechat","auth_token":"EXPIRED","device_id":"sandbox-1"}'
# 期望: {"code":401,"message":"wechat: errcode=40029 msg=invalid code"}
```

**QQ 端到端**:
```bash
curl -s -X POST http://127.0.0.1:8081/api/login -H 'Content-Type: application/json' \
  -d '{"zone_id":1,"auth_type":"qq","auth_token":"qq_token_1","device_id":"sandbox-1"}'
# 期望: code:0,Redis 里有 account_data:qq_unionid_*

curl -s -X POST http://127.0.0.1:8081/api/login -H 'Content-Type: application/json' \
  -d '{"zone_id":1,"auth_type":"qq","auth_token":"INVALID","device_id":"sandbox-1"}'
# 期望: code:401 message:"qq: error=100007 ..."
```

### G.4 从 mock 切回真 API

只需:
1. 拿到真 `AppId` / `AppSecret`,放进 K8s Secret(或 env var)
2. **删掉 `Endpoint:` 这行**(或留空字符串)— 代码会自动回 `https://api.weixin.qq.com` / `https://graph.qq.com`
3. 重启 login

整个 chain 的 wire format 完全没变,所以 mock 验过的同一组 curl 命令直接对着真 API 也能跑通。

> **何时该用 mock vs 真 API?**
> - **CI / 集成测试 / 不可联网的环境** → mock(deterministic + 离线)
> - **真凭证已就绪 + 网络出口已开 + 准备最终验收** → 真 API(只验 mock 没法覆盖的:网络白名单 / 限流 / 真 unionid 政策)
> 二者都跑过一遍,才算 §C.4 真正打勾。

---

## 关联文档
- [auth-provider-framework.md](../design/auth-provider-framework.md) §How to Add — 加新 provider 的 4 步骤(Apple Sign-in / Google Play Games 都按这个加)
- [third-party-login-end-to-end-design.md](../design/third-party-login-end-to-end-design.md) — 完整设计 + UserBindings 表(账号合并未来要用)
- [dual-token-authentication.md](../design/dual-token-authentication.md) — 第三方 token / access / refresh 三层关系
- 代码: `go/login/internal/logic/pkg/auth/providers.go`(WeChatProvider / QQProvider)
- 代码: `go/login/internal/svc/auth_init.go`(provider 注册)
- 代码: `java/gateway_node/.../grpc/LoginRpcClient.java`(auth_type/auth_token wire 透传)
