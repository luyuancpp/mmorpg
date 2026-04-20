# Dual Token Authentication Design (Access + Refresh)

**Date:** 2026-04-20
**Status:** Implemented

## Problem

1. Gate connection token (HMAC) TTL was 60s in Java Gateway, causing `token expired` errors when client connects slightly late.
2. After login via third-party (WeChat/QQ/SA-Token), there's no persistent auth credential. If the player disconnects, they must redo the full OAuth flow.
3. No "remember me" capability -- typical WeChat/QQ games allow silent re-login for 30 days.

## Architecture

### Three-Layer Token System

```
Layer 1: Third-Party Auth (WeChat/QQ/SA-Token/Password)
  |
  | (first login only, validates once)
  v
Layer 2: Access Token (2h) + Refresh Token (30d)   <-- NEW
  |
  | (issued after successful auth, stored in Redis)
  v
Layer 3: Gate Token (HMAC, 300s)
  |
  | (one-time connection handshake, unchanged)
  v
  TCP Session (Gate)
```

### Token Comparison

| Token | Type | TTL | Purpose | Storage |
|-------|------|-----|---------|---------|
| Third-party token | OAuth code / SA-Token | Varies | Initial auth | Third-party Redis |
| **Access Token** | Opaque (base64url, 256-bit) | **2 hours** | Reconnect without re-auth | Redis: `access_token:{value}` |
| **Refresh Token** | Opaque (base64url, 256-bit) | **30 days** | Silent access token renewal | Redis: `refresh_token:{value}` |
| Gate Token | HMAC-SHA256 signed protobuf | 300 seconds | One-time gate TCP handshake | Not stored (verified by signature) |

## Flow Diagrams

### First Login (WeChat Example)

```
Client                  Java Gateway           Go Login            Redis
  |                         |                     |                  |
  |-- POST /assign-gate --->|                     |                  |
  |<-- gate_ip + HMAC token |                     |                  |
  |                         |                     |                  |
  |== TCP connect to Gate ==========================|                |
  |-- ClientTokenVerify --->|                     |                  |
  |<-- verified             |                     |                  |
  |                         |                     |                  |
  |-- Login(auth_type:"wechat", auth_token:code) -->|               |
  |                         |      WeChat OAuth API validates code   |
  |                         |                     |-- SET access_token:xxx ->|
  |                         |                     |-- SET refresh_token:yyy ->|
  |<-- LoginResponse{players, access_token, refresh_token} ---------|
```

### Reconnect (Within 2h)

```
Client                         Go Login            Redis
  |                               |                  |
  |-- Login(auth_type:"access_token", auth_token:xxx) -->|
  |                               |-- GET access_token:xxx ->|
  |                               |<-- {account, auth_type}   |
  |<-- LoginResponse{players} ----|                  |
```

### Token Refresh (Access Expired, Refresh Valid)

```
Client                         Go Login            Redis
  |                               |                  |
  |-- RefreshToken(refresh_token:yyy) ->|            |
  |                               |-- GET refresh_token:yyy ->|
  |                               |<-- {account, auth_type}   |
  |                               |-- DEL refresh_token:yyy -->|  (one-time use)
  |                               |-- SET access_token:xxx2 -->|
  |                               |-- SET refresh_token:yyy2 ->|
  |<-- RefreshTokenResponse{new_access, new_refresh} |
  |                               |                  |
  |-- Login(auth_type:"access_token", auth_token:xxx2) -->|
```

### Full Re-Login (Both Tokens Expired, After 30 Days)

```
Client must redo WeChat/QQ OAuth flow from scratch.
```

## Redis Key Schema

| Key Pattern | Value | TTL | Purpose |
|-------------|-------|-----|---------|
| `access_token:{token}` | JSON `{account, auth_type, device_id, created_at}` | 2h | Access token lookup |
| `refresh_token:{token}` | JSON `{account, auth_type, device_id, created_at}` | 30d | Refresh token lookup |
| `account_refresh:{account}` | SET of refresh token strings | 30d | Bulk revocation (password change) |

## Security Properties

1. **Token rotation**: Refresh token is one-time-use. Each refresh issues a new pair and invalidates the old refresh token.
2. **Race protection**: `DEL` is atomic -- if two requests try to use the same refresh token, only one succeeds.
3. **Bulk revocation**: `RevokeAll(account)` deletes all refresh tokens for an account (e.g. on password change).
4. **Opaque tokens**: 256-bit crypto/rand, base64url encoded. Not guessable, not decodable.
5. **Gate token unchanged**: HMAC-SHA256 gate handshake is orthogonal to auth tokens.

## Configuration

### Go Login (`go/login/etc/login.yaml`)

```yaml
TokenConfig:
  AccessTokenTTL: 2h      # Access token lifetime
  RefreshTokenTTL: 720h   # Refresh token lifetime (30 days)
```

### Java Gateway (`application.yaml`)

```yaml
gate:
  token-ttl-seconds: 300   # Gate handshake token (was 60, now matches Go Login)
```

## Files Changed

| File | Change |
|------|--------|
| `proto/login/login.proto` | Added `access_token`, `refresh_token` fields to `LoginResponse`; added `RefreshTokenRequest/Response` messages and `RefreshToken` RPC |
| `go/login/internal/logic/pkg/token/token.go` | **NEW** - Token manager: Issue, ValidateAccess, Refresh, RevokeAll |
| `go/login/internal/logic/pkg/auth/providers.go` | Added `AccessTokenProvider` for `auth_type:"access_token"` |
| `go/login/internal/logic/clientplayerlogin/loginlogic.go` | Issue tokens on successful login (skip for access_token re-auth) |
| `go/login/internal/logic/clientplayerlogin/refreshtokenlogic.go` | **NEW** - RefreshToken RPC handler |
| `go/login/internal/server/clientplayerlogin/clientplayerloginserver.go` | Added `RefreshToken` method |
| `go/login/internal/config/config.go` | Added `TokenConf` struct |
| `go/login/internal/svc/servicecontext.go` | Added `TokenManager` to ServiceContext |
| `go/login/internal/svc/auth_init.go` | Register `access_token` provider |
| `go/login/etc/login.yaml` | Added `TokenConfig` section |
| `java/gateway_node/.../application.yaml` | `token-ttl-seconds: 60` → `300` |
| `robot/pkg/client.go` | Added `AccessToken`, `RefreshToken` fields |
| `robot/login.go` | Store received tokens for reconnect |

## Industry Reference (WeChat/QQ Games)

| Platform | Access Token TTL | Refresh Token TTL | Notes |
|----------|-----------------|-------------------|-------|
| WeChat Mini Game | 2h (session_key) | 30d (via wx.login) | session_key expires, wx.login silently renews |
| QQ Connect | 2h | 30d | Standard OAuth2 |
| Apple Game Center | 48h | 60d | Longer due to platform constraints |
| Google Play Games | 1h | Indefinite (revocable) | Uses Google OAuth2 |
| Our implementation | **2h** | **30d** | Matches WeChat/QQ standard |
