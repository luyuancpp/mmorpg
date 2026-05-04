# Auth Provider Framework

## Overview

Login service supports multiple third-party authentication methods via a strategy pattern.
Each auth method is a `Provider` implementation, registered at startup based on YAML config.
The `password` provider is always available; others (WeChat, QQ, NetEase, SA-Token) are enabled by adding their config section.

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
- Password login: set `account` + `password`, leave `auth_type` empty or `"password"`.
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

// Registry
auth.Register(name string, p Provider)  // Register a provider (panics on duplicates)
auth.Get(name string) Provider          // Lookup by name (returns nil if not found)
```

## Built-in Providers

| auth_type   | Provider           | Token source                | Status        |
|-------------|--------------------|-----------------------------|---------------|
| `password`  | `PasswordProvider` | `account` field directly    | Implemented   |
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
  return in.Account      provider = auth.Get(auth_type)
                                |
                         provider.Validate(ctx, auth_token)
                                |
                         return AuthResult.Account
                |
                v
   (continue normal login flow: lock, session, device limit, etc.)
```

## Config (login.yaml)

`password` provider needs no config. Others are enabled by adding their section under `Auth`:

```yaml
# All optional. Omit a section to disable that provider.
Auth:
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
│   ├── provider.go        # Provider interface + registry (Register/Get)
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
