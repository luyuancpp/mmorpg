package main

import (
	"github.com/luyuancpp/muduoclient-new/muduo"
	"github.com/panjf2000/ants/v2"
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
	"robot/proto/service/go/grpc/login"
	"strconv"
	"sync"
	"time"
)

// runClientLoop：适配回调模式，无chan、无原生go func
func runClientLoop(gameClient *pkg.GameClient, pool *ants.Pool, globalWg *sync.WaitGroup) {
	// 1. 注册消息回调（替代原 msgCh 接收逻辑）
	gameClient.Client.SetMessageCallback(func(msg proto.Message, connCtx *muduo.ConnContext) {
		// 提交消息处理任务到 ants 池，避免阻塞 gnet IO 线程
		err := pool.Submit(func() {
			defer func() {
				if r := recover(); r != nil {
					zap.L().Error("Message callback task panicked",
						zap.String("account", gameClient.Account),
						zap.String("connID", connCtx.ConnID),
						zap.Any("panic", r))
				}
			}()

			// 原消息处理逻辑完全复用（去掉 string(d.Name()) 的冗余转换）
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
				player, ok := gameobject.PlayerList.Get(gameClient.PlayerId)
				if ok {
					player.TickBehaviorTree()
				}
			default:
				zap.L().Warn("Unhandled message type",
					zap.String("message_type", string(d.Name())), // d.Name() 本身是 string，无需转换
					zap.String("account", gameClient.Account),
					zap.String("connID", connCtx.ConnID))
			}
		})
		if err != nil {
			zap.L().Error("Submit message callback task failed",
				zap.String("account", gameClient.Account),
				zap.String("connID", connCtx.ConnID),
				zap.Error(err))
		}
	})

	// 2. 定时 Tick 任务（提交到 ants 池，替代原 time.After 循环）
	globalWg.Add(1)
	err := pool.Submit(func() {
		defer globalWg.Done()
		// 从配置读取 Tick 间隔，确保非负（避免异常）
		tickInterval := time.Duration(config.AppConfig.Robots.Tick) * time.Millisecond
		if tickInterval <= 0 {
			tickInterval = 1000 * time.Millisecond // 兜底默认1秒
			zap.L().Warn("Invalid tick interval, use default",
				zap.Int64("config_tick", config.AppConfig.Robots.Tick),
				zap.String("account", gameClient.Account))
		}
		ticker := time.NewTicker(tickInterval)
		defer ticker.Stop()

		for {
			select {
			case <-ticker.C:
				// 客户端已关闭，退出 Tick 任务
				if gameClient.Client.IsClosed() {
					zap.L().Info("Tick task exited: client closed", zap.String("account", gameClient.Account))
					return
				}
				// 原 Tick 逻辑复用
				player, ok := gameobject.PlayerList.Get(gameClient.PlayerId)
				if ok {
					player.TickBehaviorTree()
				} else {
					gameClient.TickBehaviorTree()
				}
			case <-gameClient.Client.Ctx().Done():
				// 感知客户端关闭信号，优雅退出
				zap.L().Info("Tick task exited: client context done", zap.String("account", gameClient.Account))
				return
			}
		}
	})
	if err != nil {
		zap.L().Error("Submit tick task failed", zap.String("account", gameClient.Account), zap.Error(err))
		globalWg.Done()
	}
}

func main() {
	// pprof 监控保持不变（仅1个原生goroutine，无影响）
	go func() {
		log.Printf("pprof server start at localhost:6060")
		log.Fatal(http.ListenAndServe("localhost:6060", nil))
	}()

	// 日志初始化：使用配置的日志级别（原代码未生效，修正）
	lvl := zap.NewAtomicLevel()
	lvl.SetLevel(zapcore.Level(config.AppConfig.LogLevel))
	// 初始化日志时应用配置级别（原代码用了默认 Production 配置，未关联 lvl）
	loggerConfig := zap.NewProductionConfig()
	loggerConfig.Level = lvl
	logger, err := loggerConfig.Build()
	if err != nil {
		panic(err)
	}
	defer logger.Sync()
	zap.ReplaceGlobals(logger)

	// 打印启动配置，方便排查
	zap.L().Info("Robot program start",
		zap.Int("robot_count", config.AppConfig.Robots.Count),
		zap.Int("max_concurrent", config.AppConfig.Robots.MaxConcurrent),
		zap.Int64("tick_interval_ms", config.AppConfig.Robots.Tick),
		zap.Int64("login_interval_ms", config.AppConfig.Robots.LoginInterval),
		zap.Int("log_level", config.AppConfig.LogLevel),
		zap.Int("server_count", len(config.AppConfig.Servers)))

	// ========== 初始化 ants 全局池 ==========
	poolSize := config.AppConfig.Robots.MaxConcurrent
	if poolSize <= 0 {
		poolSize = config.AppConfig.Robots.Count * 2 // 更合理的默认值：机器人数量×2
		zap.L().Warn("MaxConcurrent not configured, use default",
			zap.Int("default_pool_size", poolSize),
			zap.Int("robot_count", config.AppConfig.Robots.Count))
	}
	// 关键修正：WithMaxBlockingTasks 前面加 ants. 前缀（原代码缺少，会编译错误）
	pool, err := ants.NewPool(poolSize,
		ants.WithPreAlloc(false),        // 按需创建goroutine，节省内存
		ants.WithMaxBlockingTasks(2000), // 修正：添加 ants. 前缀
		ants.WithNonblocking(false),     // 任务满时阻塞等待，不丢弃
		ants.WithPanicHandler(func(i interface{}) {
			zap.L().Error("Ants pool task panicked", zap.Any("panic", i))
		}), // 全局捕获池内任务panic
	)
	if err != nil {
		zap.L().Fatal("Create ants pool failed", zap.Error(err))
	}
	defer pool.Release() // 程序退出释放池资源

	// ========== 全局 WaitGroup：管理所有任务生命周期 ==========
	var globalWg sync.WaitGroup

	// 检查服务器配置是否为空（避免 panic）
	if len(config.AppConfig.Servers) == 0 {
		zap.L().Fatal("No servers configured")
	}

	// ========== 提交机器人主任务到 ants 池 ==========
	for i := 0; i < config.AppConfig.Robots.Count; i++ {
		// 登录间隔：确保非负（避免异常）
		loginInterval := time.Duration(config.AppConfig.Robots.LoginInterval) * time.Millisecond
		if loginInterval > 0 {
			time.Sleep(loginInterval)
		}
		globalWg.Add(1)

		idx := i // 捕获循环变量，避免闭包陷阱
		err := pool.Submit(func() {
			defer func() {
				globalWg.Done() // 机器人主任务结束，释放Wg
				if r := recover(); r != nil {
					zap.L().Error("Robot main task panicked", zap.Int("index", idx), zap.Any("panic", r))
				}
			}()

			// 机器人服务器分配（原逻辑不变）
			serverIndex := idx % len(config.AppConfig.Servers)
			server := config.AppConfig.Servers[serverIndex]

			// 创建改造后的 muduo TcpClient（回调模式）
			client := muduo.NewTcpClient(server.Address, &muduo.TcpCodec{})
			gameClient := pkg.NewGameClient(client)
			defer gameClient.Close() // 任务结束优雅关闭客户端

			// 分配机器人账号
			gameClient.Account = "luhailong" + strconv.Itoa(idx)
			zap.L().Info("Robot client initialized",
				zap.String("account", gameClient.Account),
				zap.String("server", server.Address),
				zap.Int("server_index", serverIndex))

			// 执行客户端循环：传入池和Wg，统一管理任务
			runClientLoop(gameClient, pool, &globalWg)
		})
		if err != nil {
			zap.L().Error("Submit robot main task failed", zap.Int("index", idx), zap.Error(err))
			globalWg.Done() // 提交失败释放Wg，避免死锁
		}
	}

	// 等待所有任务完成（机器人主任务+消息处理任务+Tick任务）
	globalWg.Wait()
	zap.L().Info("All robot tasks completed, program exiting")
}
