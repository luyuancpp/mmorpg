module player_inbox

// 仓库统一使用 Go 1.27.2;schemamigrate(proto2mysql v0.2.0)最低要求 1.26.5(D-14 理由 3)。
go 1.27.2

replace proto => ../proto

replace shared => ../shared

replace schemamigrate => ../schemamigrate

// go.sum 与 indirect 依赖块刻意不手写,由 Codex 执行 go mod tidy 生成(AGENTS.md §10.1)。
// 依赖与 go/trade 同版本;kafka-go 与 go/match 同版本(在线提醒经 scene-cmd 写 Kafka)。
require (
	github.com/go-sql-driver/mysql v1.9.3
	github.com/prometheus/client_golang v1.23.2
	// 核心 Redis:读 player:{id}:owner_epoch(围栏上界)、player:{id}:location(在线提醒),以及维护选主锁。
	github.com/redis/go-redis/v9 v9.17.3
	github.com/segmentio/kafka-go v0.4.50
	github.com/zeromicro/go-zero v1.10.0
	go.etcd.io/etcd/client/v3 v3.5.15
	google.golang.org/grpc v1.79.3
	google.golang.org/protobuf v1.36.11
	proto v0.0.0
	schemamigrate v0.0.0
	shared v0.0.0
)

// replace 不随依赖传递:与 schemamigrate 同步固定迁名后的发布包,保留旧 module/import 身份(同 go/trade)。
replace github.com/luyuancpp/proto2mysql v0.2.0 => github.com/luyuan-cpp/proto2mysql v0.2.0
