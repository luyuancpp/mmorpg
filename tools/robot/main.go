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

			// 原消息处理逻辑完全复用
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
					zap.String("message_type", d.Name()),
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
		ticker := time.NewTicker(time.Duration(config.AppConfig.Robots.Tick) * time.Millisecond)
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
	// pprof 监控保持不变
	go func() {
		log.Fatal(http.ListenAndServe("localhost:6060", nil))
	}()

	// 日志初始化保持不变
	logger, err := zap.NewProduction()
	if err != nil {
		panic(err)
	}
	defer logger.Sync()
	zap.ReplaceGlobals(logger)

	lvl := zap.NewAtomicLevel()
	lvl.SetLevel(zapcore.Level(config.AppConfig.LogLevel))

	// ========== 初始化 ants 全局池 ==========
	poolSize := config.AppConfig.Robots.MaxConcurrent
	if poolSize <= 0 {
		poolSize = 200 // 默认并发数（每个机器人2个任务：主任务+Tick任务）
	}
	pool, err := ants.NewPool(poolSize,
		ants.WithPreAlloc(false),    // 按需创建goroutine，节省内存
		WithMaxBlockingTasks(2000),  // 最大排队任务数（适配大量机器人）
		ants.WithNonblocking(false), // 任务满时阻塞等待，不丢弃
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

	// ========== 提交机器人主任务到 ants 池 ==========
	for i := 0; i < config.AppConfig.Robots.Count; i++ {
		// 登录间隔：避免服务器瞬间压力过大
		time.Sleep(time.Duration(config.AppConfig.Robots.LoginInterval) * time.Millisecond)
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
				zap.String("server", server.Address))

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
