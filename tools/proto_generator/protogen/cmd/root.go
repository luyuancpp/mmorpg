package main

import (
	"fmt"
	"net/http"
	_ "net/http/pprof"
	"os"
	"os/signal"
	_config "protogen/internal/config"
	"protogen/logger"
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
		panic(fmt.Sprintf("Failed to initialize global logger: %v", err))
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
	logger.Global.Info("Waiting for shutdown. Visit localhost:11111/debug/pprof for profiling. Press Ctrl+C to exit.")
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)
	<-sigChan
	logger.Global.Info("Graceful shutdown started...")
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
	enablePprof := isEnabledEnv("PROTOGEN_ENABLE_PPROF") || isEnabledEnv("PBGEN_ENABLE_PPROF")
	if enablePprof {
		startPprofServer()
	}

	if err := _config.Load(); err != nil {
		logger.Global.Fatal("Failed to load config", zap.Error(err))
	}

	logger.Global.Info("Config loaded", zap.String("output_root", _config.Global.Paths.OutputRoot))

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

	logger.Global.Info("Generation complete. To enable profiling, set PROTOGEN_ENABLE_PPROF=1 (legacy: PBGEN_ENABLE_PPROF=1)")
}
