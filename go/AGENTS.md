# GO KNOWLEDGE BASE

## OVERVIEW
`go/` contains multiple service roots rather than one monolith: login, scene manager, data service, player locator, db, contracts, and generated service-local artifacts.

## STRUCTURE
```text
go/
├── login/           # Login grpc service
├── scene_manager/   # Scene manager grpc service
├── data_service/    # Data service grpc service
├── player_locator/  # Player location service
├── db/              # DB-related service/module
├── contracts/       # Shared Go-side contracts/helpers
└── generated/       # Generated Go outputs
```

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Login bootstrap | `login/login.go` | etcd registration + unary interceptor |
| Scene manager bootstrap | `scene_manager/scenemanagerservice.go` | load reporter + zrpc server |
| Data service bootstrap | `data_service/dataservice.go` | standard go-zero RPC server wiring |
| Config/runtime flags | `*/etc/` + entry file flags | Reflection only in dev/test |
| Generated RPC stubs | `*/proto/` and `generated/` | Review only; regenerate instead of patching |

## CONVENTIONS
- Services follow go-zero / `zrpc.MustNewServer` startup.
- Reflection is enabled only for dev/test mode checks.
- `login` owns etcd-backed node registration behavior; trace service-discovery issues from `login.go` first.
- Contract changes should be regenerated through repo tooling, not fixed manually inside generated Go files.
- Service-local `proto/` directories and top-level `generated/` are outputs, not primary authoring targets.

## ANTI-PATTERNS
- Hand-editing generated `*.pb.go` / grpc outputs.
- Copying startup wiring between services without preserving each service's config flag shape.
- Treating `go/` as one module with uniform commands; each service root may have its own `go.mod` and lifecycle.

## COMMANDS
```bash
cd go && build.bat
cd go\login && go run login.go
cd go\login && go test ./...
cd go\login && go mod tidy
cd go\scene_manager && go test ./...
cd go\data_service && go test ./...
```

## NOTES
- `build.bat` mixes `goctl rpc protoc` and raw `protoc`; it is the canonical regeneration path found in-repo.
- `data_service` and `scene_manager` follow the same high-level go-zero server shape as login, but only login handles node registration directly.
- When `.proto` changes, regenerate first, then rebuild affected Go services.
