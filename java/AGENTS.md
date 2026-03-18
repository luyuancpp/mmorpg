# JAVA KNOWLEDGE BASE

## OVERVIEW
`java/` contains auth/web-side Spring Boot services and starter examples. The main production-facing subtree here is `sa_token_node/`.

## STRUCTURE
```text
java/
├── sa_token_node/                  # Spring Boot + grpc auth node
└── springboot_satoken_auth_starter/ # Example/starter app with local auth stack
```

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Auth grpc service | `sa_token_node/` | Maven Spring Boot app |
| Dependency/runtime wiring | `sa_token_node/pom.xml` | Sa-Token, Nacos, Redis, grpc |
| Source tree | `sa_token_node/src/main/` | Main Java code |
| Tests | `sa_token_node/src/test/` | Maven test entry |
| Example/starter | `springboot_satoken_auth_starter/` | Standalone example, not main repo runtime |

## CONVENTIONS
- Treat `sa_token_node/` as the primary Java service in this repo.
- Use Maven commands from that subtree rather than assuming a root Java build.
- Contract changes still originate in top-level `proto/`; regenerate and rebuild consumers after changes.
- The starter project is useful as a reference/example, not as the authoritative runtime path for the MMORPG backend.

## ANTI-PATTERNS
- Editing generated contract consumers without first updating `proto/`.
- Treating `springboot_satoken_auth_starter/` as the same thing as `sa_token_node/`.
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
