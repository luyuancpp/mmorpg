# JAVA KNOWLEDGE BASE

## OVERVIEW
`java/` contains Spring Boot services: `sa_token_node/` for authentication and `gateway_node/` for the Zone Directory / Gateway service (server list, gate assignment, zone health probing, admin APIs).

## STRUCTURE
```text
java/
├── sa_token_node/                   # Spring Boot + grpc auth node
├── gateway_node/                    # Spring Boot gateway (zone directory + gate assignment)
│   ├── src/main/java/com/game/gateway/
│   │   ├── config/                  # EtcdProperties, GateProperties, AdminApiKeyFilter
│   │   ├── controller/              # Client APIs + Admin APIs
│   │   ├── dto/                     # Request/Response DTOs, enums
│   │   ├── entity/                  # JPA entities (ZoneConfig, ZoneWhitelist, Announcement)
│   │   ├── etcd/                    # GateWatcher, NodeInfoRecord, NodeType
│   │   ├── repository/              # JPA repositories
│   │   └── service/                 # Zone probe, server list, gate assign, CDN sign
│   └── src/test/                    # Unit tests
└── springboot_satoken_auth_starter/ # Example/starter app with local auth stack
```

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Auth grpc service | `sa_token_node/` | Maven Spring Boot app |
| Zone directory / gateway | `gateway_node/` | Maven Spring Boot app (port 8081) |
| Server list API | `gateway_node/.../controller/ServerListController.java` | GET /api/server-list |
| Gate assignment API | `gateway_node/.../controller/AssignGateController.java` | POST /api/assign-gate |
| Zone health probing | `gateway_node/.../service/ZoneHealthProbeService.java` | @Scheduled etcd+Redis check |
| Admin zone management | `gateway_node/.../controller/AdminZoneController.java` | CRUD + maintenance/open |
| Admin API key filter | `gateway_node/.../config/AdminApiKeyFilter.java` | X-Admin-Key header for /admin/** |
| etcd gate/scene watcher | `gateway_node/.../etcd/GateWatcher.java` | jetcd-based node discovery |
| Dependency/runtime wiring | `sa_token_node/pom.xml`, `gateway_node/pom.xml` | Sa-Token, Nacos, Redis, grpc, jetcd |
| Tests | `*/src/test/` | Maven test entry |
| Example/starter | `springboot_satoken_auth_starter/` | Standalone example, not main repo runtime |

## CONVENTIONS
- Both `sa_token_node/` and `gateway_node/` use JDK 23 and Spring Boot 3.4.3.
- `gateway_node/` replaces the former Go gateway (`go/gateway/`, removed 2026-04-14).
- Admin APIs require `X-Admin-Key` header matching `admin.api-key` in application.yaml.
- Zone status uses dual-path: manual (MySQL) + auto (etcd/Redis probe). Manual always overrides auto.
- Gate token signing uses HMAC-SHA256 matching the Go implementation for wire compatibility.
- Use Maven commands from each subtree (`cd java/gateway_node && ./mvnw test`).
- Contract changes still originate in top-level `proto/`; regenerate and rebuild consumers after changes.

## ANTI-PATTERNS
- Editing generated contract consumers without first updating `proto/`.
- Treating `springboot_satoken_auth_starter/` as the same thing as `sa_token_node/`.
- Launching the Go gateway alongside the Java gateway (they serve the same role).
- Mixing unrelated example-app settings into the auth node without verifying runtime intent.

## COMMANDS
```bash
cd java\sa_token_node && mvn clean install
cd java\sa_token_node && mvn test
cd java\sa_token_node && mvn -Dtest=GrpcServerApplicationTests#contextLoads test
cd java\springboot_satoken_auth_starter && mvn spring-boot:run
```

## NOTES
- `sa_token_node/pom.xml` currently pulls in Spring Boot, Nacos discovery, Redis, Sa-Token, and grpc starter dependencies.
- `springboot_satoken_auth_starter/README.md` documents a local example flow with Redis/H2 and optional Docker-backed production setup.
