# Auth Provider Framework

## Overview

Login service supports multiple third-party authentication methods via a strategy pattern.
Each auth method is a `Provider` implementation, registered at startup based on YAML config.
Production password authentication validates Argon2id hashes in authoritative
MySQL and is opt-in/fail-closed. Registration/recovery remain deliberately absent
from the public Login RPC; existing legacy accounts are provisioned once with a
separate no-echo migration command.
Third-party providers (WeChat, QQ, NetEase, SA-Token) are enabled explicitly.
A prefix-restricted shared-secret provider exists only for local robots/dev.

## Proto Interface

```protobuf
// proto/login/login.proto
message LoginRequest {
  string account    = 1; // Account name (used directly for password auth)
  string password   = 2; // Password (for password auth only)
  string auth_type  = 3; // Provider type: "", "password", "wechat", "qq", "netease", "satoken"
  string auth_token = 4; // Third-party code/token (used when auth_type is not "password")
}
```

**Client behavior:**
- Password login: empty `auth_type` and `"password"` are equivalent. Both require
  a configured production MySQL authenticator (or the guarded dev-only provider).
- Third-party login: set `auth_type` + `auth_token`. The `account` field is ignored; the provider resolves the account from the token.

## Go Interface

```go
// go/login/internal/logic/pkg/auth/provider.go

type AuthResult struct {
    Account string
}

type Provider interface {
    Validate(ctx context.Context, token string) (*AuthResult, error)
}

type PasswordAuthenticator interface {
    ValidatePassword(ctx context.Context, account, password string) (*AuthResult, error)
}

// Registry
auth.Register(name string, p Provider)  // Register a provider (panics on duplicates)
auth.Get(name string) Provider          // Lookup by name (returns nil if not found)
auth.RegisterPassword(p PasswordAuthenticator)
auth.GetPassword() PasswordAuthenticator // nil means fail-closed
```

## Built-in Providers

| auth_type   | Provider           | Token source                | Status        |
|-------------|--------------------|-----------------------------|---------------|
| `password`  | Production password authenticator | `account` + `password` | Implemented, default disabled; MySQL Argon2id only |
| `password` (dev only) | `DevelopmentPasswordProvider` | allowed account prefix + env-injected shared secret | Implemented, default disabled |
| `satoken`   | `SaTokenProvider`  | SA-Token Redis key lookup   | Implemented   |
| `wechat`    | `WeChatProvider`   | WeChat OAuth `code`         | Implemented (`/sns/oauth2/access_token`, account = `wx_<unionid|openid>`) |
| `qq`        | `QQProvider`       | QQ Connect `access_token`   | Implemented (`graph.qq.com/oauth2.0/me`, account = `qq_<unionid|openid>`) |
| `netease`   | `NeteaseProvider`  | NetEase auth token          | Stub (TODO)   |

## Login Flow

```
Client -> LoginRequest{auth_type, auth_token}
                |
                v
   resolveAccount(LoginRequest)
       |                        |
  auth_type == ""/"password"    auth_type == "wechat"/"satoken"/...
       |                        |
  passwordAuthenticator  provider = auth.Get(auth_type)
       |                        |
  missing => reject       provider.Validate(ctx, auth_token)
  MySQL lookup + Argon2id verify  |
       |                        |
  return verified account  return verified account
                |
                v
   (continue normal login flow: lock, session, device limit, etc.)
```

## Config (login.yaml)

Password is disabled by default; third-party-only deployments leave `Enabled`
false. Production names an environment variable containing the MySQL DSN. A
missing DSN makes startup fail rather than silently disabling verification:

```yaml
PasswordAuth:
  Enabled: true
  DSNEnv: "LOGIN_PASSWORD_AUTH_DSN"
  MaxOpenConns: 20
  MaxIdleConns: 10
  KDFConcurrency: 2
  KDFWaitTimeout: 500ms
  FailureLimit: 5
  FailureWindow: 15m
  FailureLock: 5m
```

`LOGIN_PASSWORD_AUTH_DSN` is injected by the secret manager using a login-service
database principal with `SELECT` on `user_accounts` only, for example
`login_auth:...@tcp(mysql:3306)/game?charset=utf8mb4`. Its value must never be
committed or logged. `PasswordAuth.Enabled=false` disables only password login;
OAuth/access-token providers continue to work.

Local robots may instead opt into the guarded dev provider by naming an
environment variable that contains the shared secret and by listing
non-production account prefixes. The secret value itself must never appear in
YAML, and production and development password providers are mutually exclusive:

```yaml
DevPasswordAuth:
  Enabled: true
  SharedSecretEnv: "LOGIN_DEV_PASSWORD_SHARED_SECRET"
  AllowedAccountPrefixes: ["robot_", "dev_"]
```

The service registers this provider only when go-zero `Mode` is `dev` or `test`.
Enabling it in production mode, or omitting/emptying the environment variable,
makes startup fail. `DevSkipAuth` is removed; retaining it in an old config causes
startup to fail instead of silently bypassing authentication.

Third-party providers are enabled under `AuthProviders`:

```yaml
# All optional. Omit a section to disable that provider.
AuthProviders:
  # SA-Token: validates token via Redis key lookup
  SaToken:
    TokenName: satoken          # Redis key prefix
    LoginType: login            # SA-Token login type
    Redis:
      Host: 127.0.0.1:6379
      Password: ""
      DB: 0

  # WeChat: OAuth code -> openid
  WeChat:
    AppId: "wx1234567890"
    AppSecret: "your-app-secret"

  # QQ: OAuth code -> openid
  QQ:
    AppId: "qq1234567890"
    AppKey: "your-app-key"

  # NetEase: token verification
  NetEase:
    AppKey: "your-app-key"
    AppSecret: "your-app-secret"
```

## File Layout

```
go/login/internal/
├── logic/pkg/auth/
│   ├── provider.go        # Token Provider + PasswordAuthenticator registries
│   └── providers.go       # All provider implementations
├── svc/
│   └── auth_init.go       # InitAuthProviders(): reads config, registers providers
├── config/
│   └── config.go          # AuthConfig, SaTokenAuthConf, WeChatAuthConf, etc.
└── logic/clientplayerlogin/
    └── loginlogic.go      # resolveAccount(): dispatches to provider
```

## How to Add a New Provider

1. **Add config struct** in `config.go`:
   ```go
   type MyPlatformAuthConf struct {
       ApiKey string `json:"ApiKey"`
   }
   ```
   Add field to `AuthConfig`:
   ```go
   MyPlatform *MyPlatformAuthConf `json:"MyPlatform,optional"`
   ```

2. **Implement Provider** in `providers.go`:
   ```go
   type MyPlatformProvider struct {
       ApiKey string
   }

   func (p *MyPlatformProvider) Validate(ctx context.Context, token string) (*AuthResult, error) {
       // Call platform API to verify token, get user ID
       userId, err := myplatform.VerifyToken(ctx, p.ApiKey, token)
       if err != nil {
           return nil, err
       }
       return &AuthResult{Account: userId}, nil
   }
   ```

3. **Register in `auth_init.go`**:
   ```go
   if cfg.MyPlatform != nil {
       auth.Register("myplatform", &auth.MyPlatformProvider{
           ApiKey: cfg.MyPlatform.ApiKey,
       })
       logx.Info("Auth provider registered: myplatform")
   }
   ```

4. **Add config to `login.yaml`**:
   ```yaml
   Auth:
     MyPlatform:
       ApiKey: "your-api-key"
   ```

5. **Client sends**: `LoginRequest{auth_type: "myplatform", auth_token: "token-from-platform"}`

No changes needed to login flow, proto, or other providers.

## Production Password Authority and Cache Boundary

- Authority is MySQL `user_accounts(account, password)`. `password` is an
  Argon2id PHC string (`m=65536,t=3,p=2`, random 16-byte salt, 32-byte result).
- Every password login queries MySQL and verifies with constant-time digest
  comparison. The Redis `account_data:{account}` value is only the player-list
  cache and is read **after** authentication; its existence, absence or expiry
  cannot bypass MySQL verification.
- Unknown accounts execute a dummy Argon2id calculation to reduce account
  enumeration by timing. Empty hashes, legacy plaintext, malformed hashes, and
  hashes below policy are rejected. Login never auto-upgrades them because it
  does not possess a trusted old credential migration contract.
- Argon2id memory is bounded per login process by a semaphore. Each verification
  uses 64 MiB, so the approximate KDF working set is
  `KDFConcurrency * 64 MiB`: default `2` is about 128 MiB. The code hard-rejects
  values above `8` (about 512 MiB), regardless of YAML. Waiting for a slot is
  bounded by the caller context and `KDFWaitTimeout` (default 500 ms, hard maximum
  5 s); cancellation or overload fails closed before any Redis/account/token
  side effect. Unknown-account dummy hashes use the same slots and cannot bypass
  this memory bound. Capacity must be budgeted per replica, then multiplied by
  the maximum simultaneous login replicas when estimating node memory.
- The canonical account returned to the rest of login comes from the MySQL row,
  not from client-controlled casing.

## Existing-database Deployment and Password Migration

1. Back up MySQL and stop account writes.
2. Run `go/login/model/migrations/20260803_password_auth.sql`. It first rejects
   NULL, empty, duplicate, and over-191-character accounts. It never truncates or
   merges data, then makes `account VARCHAR(191) PRIMARY KEY` and bounds the hash
   column to `VARCHAR(255)`.
3. For each existing account, run from `go/login` with the DSN secret in the
   environment:

   ```powershell
   $env:LOGIN_PASSWORD_MIGRATION_DSN = '<short-lived UPDATE credential from secret manager>'
   go run ./cmd/password_admin -account alice
   ```

   The command reads and confirms the password without terminal echo and updates
   exactly one already-existing, not-yet-Argon2id row. It refuses missing,
   duplicate, or already-migrated accounts, so it cannot become an implicit
   registration or password-reset API. For a controlled batch,
   `-password-stdin` accepts exactly two piped lines; plaintext must never be a
   command-line argument or logged.
4. Confirm migrated rows begin with `$argon2id$`. Unmigrated NULL/plaintext rows
   remain unable to use password login but can still use an independently bound
   third-party provider.
5. Inject `LOGIN_PASSWORD_AUTH_DSN`, set `PasswordAuth.Enabled: true`, deploy a
   canary, and test correct/wrong/unknown credentials. A missing DSN or unreachable
   database makes startup fail.
6. Disable the legacy Gate login path after its existing telemetry gate, so the
   Java Gateway's IP/account rate limiter is the single public ingress.

Public self-registration, password recovery, and user-driven password change are
still **not exposed**: they require a product contract, identity proof, refresh-
token revocation and audit policy across Gateway/client. Until those are designed,
operations use the explicit admin path above. In particular, “first password
presented claims an existing account” is forbidden and is not implemented.

## 2026-10-15 八域对齐：跨副本口令失败配额

本批职责是生产口令认证内部的账号失败保护，由真实密码登录入口调用，不改变注册、建角、令牌、三方认证或客户端契约。原仓 `services/account/login` 默认 5 次失败、15 分钟固定计数窗、5 分钟锁定；其 Check→KDF→Record 多命令与故障放行是背压策略，不能作为全副本严格失败上限。本仓采用以下有限状态门，开发共享口令和第三方认证不混入该配额。两版已接入生产认证工厂并验证，实际测试范围与局限见本节末尾。

### 身份、配置与调用顺序

1. 沿现有输入形状检查和权威账号查询，先取得库中 canonical 原值。已知账号按该值原始 UTF-8 的完整 SHA-256 建键，不 lower、trim 或按客户端输入另立额度；两版账号列的等价语义不同，不复制源的猜测式归一化。
2. 未知但形状合法的账号使用独立 unknown 域及原始输入哈希，仍跑既有 dummy KDF。空/非法输入继续既有 dummy 与进程内 KDF 门，不宣称任意未知账号枚举已被全局入口配额覆盖。无可信 IP 的入口不伪装成 IP 配额。
3. 生产口令启用时必须装配 Redis 配额，无故障放行或负数关闭。配置缺省沿源 5/15m/5m；limit 1..100，window/lock 均为 1s..1h 内的整毫秒值，不静默截断。单次在途租期固定 30s、单次 Redis 等待上限 250ms（有父期限取较小者）。不同副本使用同一权威 Redis 和同一配置；状态保存参数指纹，遇混配明确拒绝，不按请求方参数重置旧锁。修改参数须先排空旧密码写者并计划状态到期窗口，不清共享 Redis。
4. 账号不含明文进入 Redis key、日志或指标；键使用独立 v1 前缀及单个 hash tag。Java 由 RedisKeys 统一生成内部键，两个版本各自 namespace/存储，不混用内部状态。固定结果类别指标不得以账号/玩家/哈希作标签。

### 原子边界与有限恢复

同一 canonical 账号最多一个在途口令尝试，跨账号并行；这是每账号的安全与资源门，不是全服串行锁。两段单键 Lua 用 Redis TIME 计算绝对期限：Begin 在检查锁窗、固定失败窗及当前租约后，为随机不可复用 token 预留；Finish 仅接受当前 token 且租期未过的完成。先做权威查询，再 Begin，再受现有并发槽限制的 KDF，最后 Finish。抢不到在途资格、锁定或 Redis 不可确认均沿现有统一认证失败返回，响应内容不显式区分账号存在性或剩余次数；这不承诺所有分支耗时相同。成功只有 Finish 明确确认后才可返回 canonical。

- `mismatch` 只来自有效 PHC 的真实不匹配（或不存在账号的 dummy 结果），计一次失败；首个失败开启固定窗口，后续失败不续窗。达到 N 时清计数并开始固定锁窗，锁到期后必须重新积满 N 次，不能用一个失败无限续锁。
- `success` 清除失败窗和锁；`abort` 只释放本 token，不计失败。DB 故障、已知账号无哈希/坏 PHC、KDF 排队满、已知取消/中断、超租期都不得伪装错密。进程内 KDF 槽直到真实计算退出才释放，不能为提前回包放开内存预算。
- 令牌重复完成不重复记数；保留最近完成 token/结果至该尝试的原租期末（不续期），仅在没有新 Begin 取代时确认相同 outcome 重放，不允许换 outcome。新 Begin 清旧完成回执；过期或被取代的旧 token 不能清新 token、记失败或返回认证成功。Begin 响应未知不跑 KDF；Finish 响应未知不发登录成功，不用新 token 重做本次结算。
- 进程崩溃/无法 abort 时只占有至有限租期；过期 token 无权完成。键 TTL 覆盖失败窗、锁窗、在途租期及有限完成回执，纯拒绝不延长锁。格式/版本/参数指纹损坏失败关闭，不删除状态冒充恢复。Redis 主从异步故障不能承诺计数不可回退，本配额不是经济账本；口令权威仍来自原数据库。

本门的严格并发保证以同一 Redis 主节点及状态未丢失为前提。生产不得把配额键放进可淘汰的缓存实例；无淘汰、备份和故障切换策略是运维前置条件，不通过客户端偷偷修改共享 Redis 配置。Redis 键被运维删除、内存淘汰或异步故障回退后无法从“首次访问”中识别出来，本批不承诺这种故障下不可回退的失败总数，更不以此声明整游全球强一致已完成。

旧进程不会消费新配额，客户端协议相同也不代表新旧认证者混跑时保护成立。生产必须先排空旧密码认证入口/节点，确认所有密码写者更新，再按统一 Redis/参数启用；回退旧版会失去该保护。新键独立于原令牌和会话，不删除或重写旧 token；本批不自动执行生产切换或清库。

Java 当前 PasswordAuthenticator 没有来自传输层的 Deadline/取消参数，取消 HTTP/TCP future 不等于中断同步 KDF；本批必须检查可观察线程中断及租约过期，不能宣称已实现传输取消传播。Go 使用已有 ctx，在取消前后复核，abort 用独立有界清理或由租约恢复；两版都不得在取消后假装验证成功。

### 本批验收范围

先用现有认证接口复现无跨副本保护的旧行为，再验证正确/错密/未知账号、canonical 别名、不同账号并行、同账号唯一在途、固定窗口/锁恢复、成功清理、abort 不计、坏 PHC/DB/KDF容量/取消、租期过后旧完成、重复与相反完成、混配/坏状态、命令结果未知。真 Redis 使用两个客户端、随机自有前缀/DB，保留原认证入口装配和调用者的回归；不使用 FLUSHALL，不把替身通过写成真 Redis、生产 HA 或容量结论。完整跨副本登录/账号与 IP 入口预算及 KDF 洪泛保护仍逐项验收，不因本配额完成就登记整个 login 或 DAU/CCU 目标已完成。

### 实际接线与最终验证（2026-10-15）

Go 与 Java 的生产认证器构造均强制非空配额，真实装配注入，不保留无限制生产构造旁路。Go 由 `InitAuthProviders` 创建同地址/DB 的专用四连接硬限小池，关闭命令自动重试、启用上下文网络预算；初始化失败释放已创建资源，现有 `ServiceContext.Stop` 收口新池和只读口令库。Java 复用现有 Redisson 连接，通过本配额专属 script 视图设 `retryAttempts(0)` 和 250ms timeout，不修改共享客户端全局设置。本机 Redisson 3.50.0 的执行器确有响应超时重发分支，不能只靠取消等待假设命令不会重放；只读反编译证据保存在 `build/login-password-quota-review-20261015/`。

两版本地票据从发出 Begin 前以单调时钟保守计满 30s，Begin 确认及 Finish 前后复核；Redis TIME 与服务端 token/租期检查仍独立有效。一次成功或错密 Finish 发出后，未知结果不会由 defer/finally 再换成 abort。坏 PHC、缺哈希、容量和可观察取消保留内部 abort 语义，不靠统一客户端 2000 猜测是否记失败。固定 locked/inflight/error/obsolete 等指标无身份标签，配额故障日志有一分钟限频。

| 范围 | 最终实际证据 |
|---|---|
| Go 完整 login | 17 个测试包通过，顶层 346 通过、2 跳过、0 失败；含子项为 578 通过事件。`go build ./...`、`go vet ./...` 与 diff 检查退出 0。两跳过仅既有只读 SQL 环境烟测和 Kafka 分区环境集成，不算已运行。 |
| Go 真实接缝 | MySQL `13316` 随机 `unicode_ci` 库＋Redis `13379`，真实 `InitAuthProviders` 注册与第二生产工厂；三种数据库等价别名共享 canonical，五错后两副本均锁正确口令，其他账号正常。真 Redis 双客户端 24 并发 Begin 只成功 1 个，窗口/恢复及结算用例均通过。 |
| Java 受影响选择器 | 所选 login 认证、旧入口及 handler 71/71，RedisKeys 金样/命名空间 17/17，共 88/88，零失败、零错误、零跳过；受影响 9 reactor package 成功。此为所选用例，不是全 game-login 或全服测试。 |
| Java 真实接缝 | 本机 Redis `13379` DB9、双 Redisson 客户端、随机 namespace，6/6。实际生产认证工厂已接配额，SQL `PlayerStore` 查询为替身；不称为真 SQL 认证集成。 |

红证据均保留：Go 旧认证器跨副本算了 6 次 KDF 而非 5；Java 旧生产入口五错后仍返回 canonical；真实配置绑定发现 `PasswordAuth` 多构造器导致 enabled 未生效，现 canonical 明确 `@ConstructorBinding` 并验证自定义/非法整毫秒参数；同 token 重放 Begin 重新武装也先由真 Redis 复现，现已完成/已过期 token 均拒绝。最初 Java 测试资源的关闭写法编译问题、Go 隔离快照漏两份只读夹具导致的三项失败也保留，修正夹具后再运行，没有改断言或跳过失败用例。

Go 610 个输入与被测快照一致，11 处本批源改动及六份他人 dirty 哈希不漂移；摘要 `build/login-password-quota-20261015/summary.json`，最终日志 `login-full-test-fixtures.jsonl`，login.exe SHA-256 `E0DC0348130281DFAC8BE55038C5D8216C04EA66A77CC6078A9AC3285ACD3A95`。Java 2250 输入与快照一致，本批 12 文件 SHA 单列在 `build/login-password-quota-java-20261015/final-verification.json`，完整选择器和命令均在该文件；最终 `game-login` jar SHA-256 `a44efe19dda7c612e7c62a75052f09c19e589f81357f06edf44418eaa6dd91ee`，版本 `0.1.0-SNAPSHOT`。各 fixture 只清理自有随机库/键；未改生产、共享 Redis 参数、客户端/proto、token 或建角链。Java 传输取消传播、可信 IP 配额、Redis 故障切换、完整 live gate 跨区登录和容量仍未验收，严格性与发布前置按前文边界。
