package config

// ----------- 项目源目录相关 -----------
var (
	ProtoDirectoryNames = [...]string{
		"common/",
		"logic/component/",
		"logic/event/",
		"logic/",
		"logic/",
		"service/go/grpc/player_locator/",
		"logic/constants/",
		"middleware/etcd/",
		"service/go/grpc/login/",
		"service/go/grpc/db/",
		"service/cpp/rpc/centre/",
		"service/cpp/rpc/scene/",
		"service/cpp/rpc/gate/",
		"service/go/grpc/chat/",
		"service/go/grpc/team/",
		"service/go/grpc/mail/",
		"logic/database/",
	}

	ProtoDirs []string

	CommonProtoDirIndex         = 0
	LogicComponentProtoDirIndex = 1
	LogicEventProtoDirIndex     = 2
	LogicSharedProtoDirIndex    = 3
	LogicProtoDirIndex          = 4
	PlayerLocatorDirIndex       = 5
	ConstantsDirIndex           = 6
	EtcdProtoDirIndex           = 7
	LoginProtoDirIndex          = 8
	DbProtoDirIndex             = 9
	CenterProtoDirIndex         = 10
	RoomProtoDirIndex           = 11
	GateProtoDirIndex           = 12
	ChatProtoDirIndex           = 13
	TeamProtoDirIndex           = 14
	MailProtoDirIndex           = 15

	// gRPC 支持的语言列表
	GrpcLanguages = [...]string{"go", "python", "java", "csharp", "ruby", "node", "php", "objc", "swift"}
)
