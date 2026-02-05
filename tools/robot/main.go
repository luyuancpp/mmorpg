package main

import (
	"github.com/luyuancpp/muduoclient-new/muduo"
	"github.com/panjf2000/ants/v2" // 保留ants依赖（可用于非客户端核心逻辑的并发控制）
	"go.uber.org/zap"
	"go.uber.org/zap/zapcore"
	"google.golang.org/protobuf/proto"
	"log"
	"net/http"
	_ "net/http/pprof"
	"robot/config"
	"robot/logic/gameobject"
	"robot/logic/handler"
	"robot/pkg"
	"robot/proto/common"
	"robot/proto/service/grpc/login"
	"strconv"
	"sync"
	"time"
)

// 配置：每个goroutine承载的客户端数量（固定为1，强制一个客户端绑定一个goroutine）
var clientsPerGoroutine = 1 // 核心约束：1个goroutine仅承载1个客户端

// runClientLogic：单个客户端核心逻辑（全程在同一个goroutine执行，强绑定）
func runClientLogic(account string, serverAddr string, globalWg *sync.WaitGroup) {
	defer globalWg.Done() // 客户端生命周期结束，释放全局Wg
	defer func() {
		if r := recover(); r != nil {
			zap.L().Error("Client logic panicked (same goroutine)",
				zap.String("account", account),
				zap.Any("panic", r),
				zap.String("goroutine_bind", "fixed"))
		}
	}()

	// 1. 初始化客户端连接（在当前goroutine创建，后续操作不换协程）
	client := muduo.NewTcpClient(serverAddr, &muduo.TcpCodec{})
	gameClient := pkg.NewGameClient(client)
	defer gameClient.Close() // 仅当前goroutine能触发关闭，避免跨协程操作
	gameClient.Account = account

	zap.L().Info("Robot client initialized (fixed goroutine)",
		zap.String("account", account),
		zap.String("server", serverAddr),
		zap.String("goroutine_bind", "one-client-one-goroutine"))

	// 2. 注册消息回调（直接在当前goroutine处理，不提交到任何池，强绑定）
	gameClient.Client.SetMessageCallback(func(msg proto.Message, connCtx *muduo.ConnContext) {
		// 回调逻辑与客户端在同一个goroutine，无协程切换
		defer func() {
			if r := recover(); r != nil {
				zap.L().Error("Message callback panicked (same goroutine)",
					zap.String("account", account),
					zap.String("connID", connCtx.ConnID),
					zap.Any("panic", r),
					zap.String("goroutine_bind", "fixed"))
			}
		}()

		d := msg.ProtoReflect().Descriptor()
		switch d.Name() {
		case "LoginResponse":
			resp := msg.(*login.LoginResponse)
			handler.ClientPlayerLoginLoginHandler(gameClient, resp)
			gameClient.TickBehaviorTree()
		case "CreatePlayerResponse":
			resp := msg.(*login.CreatePlayerResponse)
			handler.ClientPlayerLoginCreatePlayerHandler(gameClient, resp)
			gameClient.TickBehaviorTree()
		case "EnterGameResponse":
			resp := msg.(*login.EnterGameResponse)
			handler.ClientPlayerLoginEnterGameHandler(gameClient, resp)
		case "MessageContent":
			resp := msg.(*common.MessageContent)
			handler.MessageBodyHandler(gameClient, resp)
		default:
			zap.L().Warn("Unhandled message type (same goroutine)",
				zap.String("message_type", string(d.Name())),
				zap.String("account", account),
				zap.String("connID", connCtx.ConnID),
				zap.String("goroutine_bind", "fixed"))
		}
	})

	// 3. 定时Tick任务（在当前goroutine内执行，无协程切换）

	tickInterval := time.Duration(config.AppConfig.Robots.Tick) * time.Millisecond
	gameClient.Client.SetHeartbeatCallback(tickInterval, func(connCtx *muduo.ConnContext) {
		if gameClient.Client.IsClosed() {
			zap.L().Info("Tick task exited: client closed (same goroutine)",
				zap.String("account", account),
				zap.String("goroutine_bind", "fixed"))
			return
		}
		// Tick逻辑与客户端在同一个goroutine，状态一致
		player, ok := gameobject.PlayerList.Get(gameClient.PlayerId)
		if ok {
			player.TickBehaviorTree()
		} else {
			gameClient.TickBehaviorTree()
		}
	})

	// 4. 阻塞主循环（客户端所有逻辑在当前goroutine完成，全程不换协程）
	for {
		select {
		case <-gameClient.Client.Ctx().Done():
			zap.L().Info("Client exited: context done (same goroutine)",
				zap.String("account", account),
				zap.String("goroutine_bind", "fixed"))
			return
		}
	}
}

func main() {
	// pprof监控保持不变
	go func() {
		log.Printf("pprof server start at localhost:6060")
		log.Fatal(http.ListenAndServe("localhost:6060", nil))
	}()

	// 日志初始化（修正后）
	lvl := zap.NewAtomicLevel()
	lvl.SetLevel(zapcore.Level(config.AppConfig.LogLevel))
	loggerConfig := zap.NewProductionConfig()
	loggerConfig.Level = lvl
	logger, err := loggerConfig.Build()
	if err != nil {
		panic(err)
	}
	defer logger.Sync()
	zap.ReplaceGlobals(logger)

	// 强制clientsPerGoroutine=1（确保一个客户端一个goroutine，不受配置影响）
	clientsPerGoroutine = 1
	// 读取配置仅作日志打印，不覆盖核心约束
	if config.AppConfig.Robots.ClientsPerGoroutine > 0 {
		zap.L().Warn("ClientsPerGoroutine config ignored (enforce one-client-one-goroutine)",
			zap.Int("config_clients_per_goroutine", config.AppConfig.Robots.ClientsPerGoroutine),
			zap.Int("actual_clients_per_goroutine", clientsPerGoroutine))
	}

	// 打印启动配置（明确标注goroutine绑定规则）
	zap.L().Info("Robot program start (one-client-one-goroutine enforced)",
		zap.Int("robot_count", config.AppConfig.Robots.Count),
		zap.Int("clients_per_goroutine", clientsPerGoroutine),
		zap.Int64("tick_interval_ms", config.AppConfig.Robots.Tick),
		zap.Int64("login_interval_ms", config.AppConfig.Robots.LoginInterval),
		zap.Int("server_count", len(config.AppConfig.Servers)),
		zap.String("goroutine_rule", "one client binds to exactly one goroutine"))

	// 初始化ants池（保留依赖，可用于后续扩展其他并发任务）
	globalPool, err := ants.NewPool(config.AppConfig.Robots.MaxConcurrent,
		ants.WithPreAlloc(false),
		ants.WithMaxBlockingTasks(500),
		ants.WithNonblocking(false),
		ants.WithPanicHandler(func(i interface{}) {
			zap.L().Error("Ants pool task panicked", zap.Any("panic", i))
		}),
	)
	if err != nil {
		zap.L().Fatal("Create global ants pool failed", zap.Error(err))
	}
	defer globalPool.Release()

	// 检查服务器配置
	if len(config.AppConfig.Servers) == 0 {
		zap.L().Fatal("No servers configured")
	}

	// 全局WaitGroup：管理所有客户端goroutine
	var globalWg sync.WaitGroup

	// 启动机器人：1个客户端对应1个goroutine（强绑定）
	for i := 0; i < config.AppConfig.Robots.Count; i++ {
		// 登录间隔控制（避免服务器瞬时压力）
		loginInterval := time.Duration(config.AppConfig.Robots.LoginInterval) * time.Millisecond
		if loginInterval > 0 {
			time.Sleep(loginInterval)
		}
		globalWg.Add(1)

		idx := i // 捕获循环变量，避免闭包陷阱
		// 为每个客户端启动独立goroutine（核心绑定逻辑）
		go func() {
			account := "luhailong" + strconv.Itoa(idx)
			serverIndex := idx % len(config.AppConfig.Servers)
			serverAddr := config.AppConfig.Servers[serverIndex].Address

			// 客户端逻辑全程在当前goroutine执行，不切换、不共享
			runClientLogic(account, serverAddr, &globalWg)
		}()
	}

	// 等待所有客户端goroutine完成
	globalWg.Wait()
	zap.L().Info("All robot goroutines finished (one-client-one-goroutine enforced), program exiting")
}
