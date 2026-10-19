# springboot-satoken-auth-starter

Complete example: Spring Boot + Sa-Token + Redis + JPA + JustAuth.

## Quick start (dev/H2)

1. Run Redis locally (optional for dev; Redis required for Sa-Token Redis storage in config).
2. Start app (the `dev` profile is required for the local-only `/auth/dev-login` and `/auth/logout` helpers that robot uses; `dev.bat start-satoken` passes it):

```
mvn spring-boot:run -Dspring-boot.run.profiles=dev
```

> **安全边界**：`/auth/dev-login` 免 OAuth、按任意账号名签发 Go login `SaTokenProvider` 信任的令牌，等于「报上账号名即登录」，
> 所以只在 Spring profile `dev` 下注册（`DevLoginController`），不带 profile 启动时这两个接口不存在（404）。
> 本 starter 没有任何部署清单，不要部署到可被玩家访问的环境；生产的微信 / QQ 登录只走 Go login 直连渠道
> （`auth_type=wechat` / `qq`），见 `docs/design/third-party-login-end-to-end-design.md`。

APIs are under `http://localhost:8080/api/auth`.

## Production with Docker

1. `docker-compose up -d` to start MySQL + Redis.
2. Edit `production_application.properties` with your credentials.
3. Build and run jar.
