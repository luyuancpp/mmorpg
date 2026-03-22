package main

import (
	"fmt"
	"net/http"
	_ "net/http/pprof"
	"os"
	"os/signal"
	_config "pbgen/internal/config"
	"pbgen/logger"
	"strings"
	"syscall"
	"time"

	"go.uber.org/zap"
)

func makeProjectDir() {
	os.MkdirAll(_config.Global.Paths.GeneratedDir, os.FileMode(0777))
	os.MkdirAll(_config.Global.Paths.ServiceInfoDir, os.FileMode(0777))
	os.MkdirAll(_config.Global.Paths.CppGenGrpcDir, os.FileMode(0777))
	os.MkdirAll(_config.Global.Paths.TempFileGenDir, os.FileMode(0775))
	os.MkdirAll(_config.Global.Paths.ProtoBufCTempDir, os.FileMode(0777))
	os.MkdirAll(_config.Global.Paths.GrpcTempDir, os.FileMode(0777))
	os.MkdirAll(_config.Global.Paths.TableGeneratorDir, os.FileMode(0777))
}

func init() {
	if err := logger.Init(); err != nil {
		panic(fmt.Sprintf("初始化全局logger失败: %v", err))
	}
}

func startPprofServer() {
	go func() {
		logger.Global.Info("Starting pprof server", zap.String("address", "localhost:11111"))
		if err := http.ListenAndServe("localhost:11111", nil); err != nil {
			logger.Global.Error("pprof server exited with error", zap.Error(err))
		}
	}()
}

func waitForShutdownSignal() {
	logger.Global.Info("程序已进入等待状态，可访问localhost:11111/debug/pprof分析，按Ctrl+C退出")
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)
	<-sigChan
	logger.Global.Info("程序开始优雅退出...")
}

func isEnabledEnv(key string) bool {
	value := strings.TrimSpace(strings.ToLower(os.Getenv(key)))
	switch value {
	case "1", "true", "yes", "on":
		return true
	default:
		return false
	}
}

func main() {
	defer logger.Sync()

	start := time.Now()
	logger.Global.Info("Program started", zap.Duration("elapsed", time.Since(start)))
	enablePprof := isEnabledEnv("PBGEN_ENABLE_PPROF")
	if enablePprof {
		startPprofServer()
	}

	if err := _config.Load(); err != nil {
		logger.Global.Fatal("配置初始化失败", zap.Error(err))
	}

	logger.Global.Info("配置加载成功", zap.String("proto根目录", _config.Global.Paths.OutputRoot))

	dir, err := os.Getwd()
	if err != nil {
		logger.Global.Fatal("Failed to get current working directory", zap.Error(err))
	}

	fmt.Println("Current working directory:", dir)

	runner := NewExecutionRunner()
	runGenerationPipeline(runner)
	runner.PrintStats()

	logger.Global.Info("Total execution time", zap.Duration("elapsed", time.Since(start)))
	if enablePprof {
		waitForShutdownSignal()
		return
	}

	logger.Global.Info("生成流程完成，默认直接退出。若需性能分析，请设置环境变量 PBGEN_ENABLE_PPROF=1")
}
