# go-zero RPC Client Timeout Fix

## Problem
go-zero v1.10.0 `RpcClientConf.Timeout` config value is NOT properly applied to outbound RPC calls.
The `TimeoutInterceptor` uses `context.WithTimeout(ctx, timeout)` which takes `min(existing_ctx_deadline, timeout)`.
Since the server-side handler context already has a 2000ms deadline (from `RpcServerConf.Timeout` default),
the client timeout is always capped at 2000ms regardless of config.

## Symptoms
- `DeadlineExceeded` errors at exactly 2001-2002ms on `SceneManager.EnterScene` calls
- Setting `SceneManagerRpc.Timeout: 5000` in config has no effect
- Using `context.WithTimeout(ctx, 5s)` also has no effect (same min behavior)

## Fix
Use `zrpc.WithCallTimeout(duration)` as a per-call option. This properly overrides the interceptor's timeout.

```go
smTimeout := time.Duration(config.AppConfig.SceneManagerRpc.Timeout) * time.Millisecond
resp, err := client.EnterScene(ctx, req, zrpc.WithCallTimeout(smTimeout))
```

## Result
- Before fix: 865/1000 (86.5%) success — all failures were `enter_fail` due to DeadlineExceeded
- After fix: **1000/1000 (100%)** success — zero enter_fail across 10 zones
- avg_login: 1.6s-2.1s, max_login: 3.5s-5.3s (all within 5000ms timeout)

## Applied in
- `go/login/internal/logic/clientplayerlogin/entergamelogic.go` (~line 260)
