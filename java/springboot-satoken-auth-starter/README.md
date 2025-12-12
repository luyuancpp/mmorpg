# springboot-satoken-auth-starter

Complete example: Spring Boot + Sa-Token + Redis + JPA + JustAuth.

## Quick start (dev/H2)

1. Run Redis locally (optional for dev; Redis required for Sa-Token Redis storage in config).
2. Start app:

```
mvn spring-boot:run
```

APIs are under `http://localhost:8080/api/auth`.

## Production with Docker

1. `docker-compose up -d` to start MySQL + Redis.
2. Edit `production-application.properties` with your credentials.
3. Build and run jar.
