# Java Gateway Portal Decision (2026-04-14)

## Decision
Use Java (Spring Boot) for the unified HTTP gateway/portal that handles:
- 公告 CRUD
- 热更版本检查
- CDN 签名下发
- 服务器列表
- 运维后台 (RBAC + CRUD)

## Why Java over Go
- **统一入口 + 过滤器链**: Spring MVC HandlerInterceptor / WebFlux WebFilter — 原生链式，声明式路由级别粒度。
- **限流**: Bucket4j / Resilience4j，开箱即用，支持 Redis 分布式限流。
- **认证**: 已有 `sa_token_node`，Sa-Token 天然支持路由拦截、踢人、Token 续签。
- **运维后台**: Sa-Token 权限注解 + MyBatis-Plus / JPA，快速出 CRUD。Go 没有对标方案。
- **决定性因素**: 运维后台 RBAC + CRUD 开发效率，Java 生态远高于 Go。

## Architecture

```
┌─────────────────────────────────────────────┐
│  Spring Boot 统一网关/门户 (单进程)          │
│                                             │
│  Filter Chain:                              │
│    → RateLimiter (Bucket4j + Redis)         │
│    → Sa-Token Auth Interceptor              │
│    → RBAC Permission Check                  │
│                                             │
│  Controllers:                               │
│    /api/announce/*        公告 CRUD          │
│    /api/server-list       服务器列表         │
│    /api/hotfix/check      热更版本检查       │
│    /api/cdn/sign          CDN 签名下发       │
│    /admin/*               运维后台 (Vue前端) │
│                                             │
│  Downstream:                                │
│    → Redis (限流计数、缓存)                  │
│    → MySQL (公告、配置持久化)                │
│    → gRPC → 现有 Go/C++ 服务 (需要时)       │
└─────────────────────────────────────────────┘
```

## Boundary

| Layer | Tech | Responsibility |
|-------|------|----------------|
| HTTP gateway/portal | Java Spring Boot | 公告、热更、CDN签名、服务器列表、运维后台 |
| Game-internal RPC | Go (go-zero) | 登录、场景管理、数据服务、玩家定位 |
| Runtime nodes | C++ | Gate、Scene、Centre 节点 |

## Key Points

1. **复用 `sa_token_node`**: 现有 Java auth 已跑通 Sa-Token + gRPC，直接在此基础上扩展。
2. **运维后台是决定性因素**: Go 做 CRUD 后台 + 权限管理的开发效率远低于 Spring Boot + Sa-Token 注解。
3. **与 Go 服务的边界**: Java 门户负责面向客户端/运维的 HTTP 层；Go 服务继续负责游戏内 gRPC 逻辑，通过 gRPC 互调。
4. **前端**: 运维后台前端用 Vue + Element Plus，Java 后端纯 REST API，前后端分离。

## Location
- Java gateway code: `java/gateway_node/` (or extend `java/sa_token_node/`)
