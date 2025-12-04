package main

import (
	"fmt"
	"net/http"
	_ "net/http/pprof"
	"os"
	"os/signal"
	"pbgen/global_value"
	"pbgen/internal"
	_config "pbgen/internal/config"
	cpp2 "pbgen/internal/generator/cpp"
	_go_option "pbgen/internal/generator/cpp/options"
	_go2 "pbgen/internal/generator/go"
	_cpp_option "pbgen/internal/generator/go/options"
	"pbgen/internal/prototools"
	proto_tools_option "pbgen/internal/prototools/option"
	"pbgen/logger" // 引入全局logger包
	"sort"
	"sync"
	"syscall"
	"time"

	"go.uber.org/zap"
)

// 移除原有局部logger变量，直接使用全局logger.Global

// WaitTimeRecord 存储等待耗时记录
type WaitTimeRecord struct {
	Name     string
	Duration time.Duration
}

var waitRecords []WaitTimeRecord
var taskRecords []WaitTimeRecord // 单独存储任务耗时

// trackFuncTime 保持不变：统计单个函数的真实执行时间
func trackFuncTime(funcName string, f func()) {
	start := time.Now()
	f() // 直接执行任务，不传递WG（WG用于控制并发，不影响耗时统计）
	elapsed := time.Since(start)
	taskRecords = append(taskRecords, WaitTimeRecord{
		Name:     funcName,
		Duration: elapsed,
	})
	logger.Global.Info("Function execution time",
		zap.String("funcName", funcName),
		zap.Duration("elapsed", elapsed),
	)
}

// 关键修改1：trackGroupTime 使用「分组局部WG」控制并发，避免全局WG干扰统计
func trackGroupTime(groupName string, tasks map[string]func(*sync.WaitGroup)) {
	var groupWg sync.WaitGroup // 分组专属WG，仅用于控制当前分组的并行任务
	start := time.Now()

	// 启动所有并行任务
	for name, task := range tasks {
		groupWg.Add(1)
		go func(taskName string, taskFunc func(*sync.WaitGroup)) {
			defer groupWg.Done()
			// 统计单个任务的真实耗时（任务执行完成才会记录）
			trackFuncTime(taskName, func() {
				taskFunc(&groupWg) // 传递分组WG给任务，确保任务内部并发正确计数
			})
		}(name, task)
	}

	// 等待所有并行任务完成，统计分组总耗时
	groupWg.Wait()
	elapsed := time.Since(start)
	waitRecords = append(waitRecords, WaitTimeRecord{
		Name:     "Group: " + groupName,
		Duration: elapsed,
	})
	logger.Global.Info("Group total execution time",
		zap.String("groupName", groupName),
		zap.Duration("elapsed", elapsed),
	)
}

// 关键修改2：trackSerialGroupTime 也使用「分组局部WG」，保持一致性
func trackSerialGroupTime(groupName string, tasks []struct {
	FuncName string
	Func     func(*sync.WaitGroup)
}) {
	var groupWg sync.WaitGroup // 串行分组的WG，仅用于任务内部并发控制
	start := time.Now()

	// 串行执行所有任务
	for _, task := range tasks {
		trackFuncTime(task.FuncName, func() {
			task.Func(&groupWg)
			groupWg.Wait()             // 等待当前任务内部的并发完成
			groupWg = sync.WaitGroup{} // 重置WG，避免累计
		})
	}

	// 统计串行分组总耗时（所有任务耗时之和）
	elapsed := time.Since(start)
	waitRecords = append(waitRecords, WaitTimeRecord{
		Name:     "Group: " + groupName,
		Duration: elapsed,
	})
	logger.Global.Info("Serial Group total execution time",
		zap.String("groupName", groupName),
		zap.Duration("elapsed", elapsed),
	)
}

func MakeProjectDir(_ *sync.WaitGroup) {
	os.RemoveAll(_config.Global.Paths.ProjectGeneratedDir)
	os.MkdirAll(_config.Global.Paths.ProjectGeneratedDir, os.FileMode(0777))

	// 无并发操作，WG参数仅为统一函数签名
	os.MkdirAll(_config.Global.Paths.GeneratedDir, os.FileMode(0777))
	os.MkdirAll(_config.Global.Paths.ServiceInfoDir, os.FileMode(0777))
	os.MkdirAll(_config.Global.Paths.CppGenGrpcDir, os.FileMode(0777))
	os.MkdirAll(_config.Global.Paths.TempFileGenDir, os.FileMode(0775))
	os.MkdirAll(_config.Global.Paths.ProtoBufCTempDir, os.FileMode(0777))
	os.MkdirAll(_config.Global.Paths.GrpcTempDir, os.FileMode(0777))
	os.MkdirAll(_config.Global.Paths.TableGeneratorDir, os.FileMode(0777))

	for i := 0; i < len(_config.Global.PathLists.ProtoDirectories); i++ {
		global_value.ProtoDirs = append(global_value.ProtoDirs, _config.Global.Paths.ProtoDir+_config.Global.PathLists.ProtoDirectories[i])
	}
}

// 保留原有工具函数（如需单独使用）
func waitWithTiming(wg *sync.WaitGroup, name string) time.Duration {
	start := time.Now()
	wg.Wait()
	elapsed := time.Since(start)

	waitRecords = append(waitRecords, WaitTimeRecord{
		Name:     name,
		Duration: elapsed,
	})

	logger.Global.Info("Wait time",
		zap.String("name", name),
		zap.Duration("elapsed", elapsed),
	)
	return elapsed
}

// 打印统计信息（保持不变）
func printStats() {
	fmt.Println("\n=== Group & Function Time Statistics ===")
	if len(waitRecords) > 0 {
		sortedWaits := make([]WaitTimeRecord, len(waitRecords))
		copy(sortedWaits, waitRecords)
		sort.Slice(sortedWaits, func(i, j int) bool {
			return sortedWaits[i].Duration > sortedWaits[j].Duration
		})

		for i, record := range sortedWaits {
			prefix := "  "
			if i == 0 {
				prefix = "\033[31m→ "
			}
			fmt.Printf("%s%d. %-60s %s\033[0m\n", prefix, i+1, record.Name, record.Duration)
		}
	}

	fmt.Println("\n=== Individual Function Time Statistics ===")
	if len(taskRecords) > 0 {
		sortedTasks := make([]WaitTimeRecord, len(taskRecords))
		copy(sortedTasks, taskRecords)
		sort.Slice(sortedTasks, func(i, j int) bool {
			return sortedTasks[i].Duration > sortedTasks[j].Duration
		})

		for i, record := range sortedTasks {
			prefix := "  "
			if i == 0 {
				prefix = "\033[33m→ "
			}
			fmt.Printf("%s%d. %-60s %s\033[0m\n", prefix, i+1, record.Name, record.Duration)
		}
	}
}

// 初始化全局logger（项目启动时只执行一次）
func init() {
	// 初始化全局logger
	if err := logger.Init(); err != nil {
		panic(fmt.Sprintf("初始化全局logger失败: %v", err))
	}

	// 移除原有局部logger初始化，改为使用全局logger
	// 配置包不再需要单独注入logger，直接使用全局logger.Global
	// _config.InitLogger(logger.Global) // 这行可以删除，因为_config包已改为直接使用全局logger
}

func main() {
	defer logger.Sync() // 程序退出时同步日志到磁盘

	start := time.Now()

	logger.Global.Info("Program started", zap.Duration("elapsed", time.Since(start)))

	go func() {
		logger.Global.Info("Starting pprof server", zap.String("address", "localhost:11111"))
		if err := http.ListenAndServe("localhost:11111", nil); err != nil {
			logger.Global.Error("pprof server exited with error", zap.Error(err))
		}
	}()

	if err := _config.Load(); err != nil {
		logger.Global.Fatal("配置初始化失败", zap.Error(err))
	}

	logger.Global.Info("配置加载成功", zap.String("proto根目录", _config.Global.Paths.OutputRoot))

	dir, err := os.Getwd()
	if err != nil {
		logger.Global.Fatal("Failed to get current working directory", zap.Error(err))
	}

	fmt.Println("Current working directory:", dir)

	// 分组1：初始化项目目录（串行，无依赖）
	trackFuncTime("MakeProjectDir", func() {
		MakeProjectDir(nil)
	})

	// 分组2：Proto文件处理（串行：先拷贝，再AddGoPackage）
	trackSerialGroupTime("ProtoFilePreparation", []struct {
		FuncName string
		Func     func(*sync.WaitGroup)
	}{
		{FuncName: "prototools.CopyProtoToGenDir", Func: prototools.CopyProtoToGenDir},
	})

	trackSerialGroupTime("ProtoFilePreparation", []struct {
		FuncName string
		Func     func(*sync.WaitGroup)
	}{
		{FuncName: "_go2.AddGoPackageToProtoDir", Func: _go2.AddGoPackageToProtoDir},
	})

	// 分组3：GRPC生成与ServiceId读取（并行）- 现在会正确统计每个任务耗时
	trackGroupTime("GRPCGeneration", map[string]func(*sync.WaitGroup){
		"cpp2.GenerateGameGrpc":  cpp2.GenerateGameGrpc,
		"cpp2.ReadServiceIdFile": cpp2.ReadServiceIdFile,
	})

	// 分组4：描述符生成（依赖分组2和3）
	trackFuncTime("prototools.GenerateAllInOneDescriptor", func() {
		var wg sync.WaitGroup
		prototools.GenerateAllInOneDescriptor(&wg)
		wg.Wait()
	})

	// 分组5：服务信息读取（依赖分组4）
	trackFuncTime("cpp2.ReadAllProtoFileServices", func() {
		var wg sync.WaitGroup
		cpp2.ReadAllProtoFileServices(&wg)
		wg.Wait()
	})

	// 合并分组1：编译+工具类+事件处理器生成（并行）
	trackGroupTime("CompilationAndUtilGeneration", map[string]func(*sync.WaitGroup){
		"cpp2.BuildProtocCpp":           cpp2.BuildProtocCpp,
		"_go2.BuildGrpcServiceProto":    _go2.BuildGrpcServiceProto,
		"cpp2.GenNodeUtil":              cpp2.GenNodeUtil,
		"cpp2.GenerateAllEventHandlers": cpp2.GenerateAllEventHandlers,
	})

	// 分组6：ServiceId初始化与写入（串行）
	trackFuncTime("cpp2.InitServiceId", func() {
		cpp2.InitServiceId()
	})
	trackFuncTime("cpp2.WriteServiceIdFile", func() {
		cpp2.WriteServiceIdFile()
	})

	// 分组7：方法文件与处理器生成（并行）
	trackGroupTime("MethodFilesGeneration", map[string]func(*sync.WaitGroup){
		"cpp2.WriteMethodFile":  cpp2.WriteMethodFile,
		"cpp2.GeneratorHandler": cpp2.GeneratorHandler,
	})

	// 合并分组2：常量、消息ID生成 + 选项构建（并行）
	trackGroupTime("ConstantsMessageIdsAndOptionBuilding", map[string]func(*sync.WaitGroup){
		"internal.GenerateServiceConstants": internal.GenerateServiceConstants,
		"internal.WriteGoMessageId":         internal.WriteGoMessageId,
		"_go_option.BuildOption": func(wg *sync.WaitGroup) {
			localWg := sync.WaitGroup{}
			localWg.Add(1)
			go func() { defer localWg.Done(); _go_option.BuildOption() }()
			localWg.Wait()
		},
		"_cpp_option.BuildOption": func(wg *sync.WaitGroup) {
			localWg := sync.WaitGroup{}
			localWg.Add(1)
			go func() { defer localWg.Done(); _cpp_option.BuildOption() }()
			localWg.Wait()
		},
	})

	// 合并分组3：最终任务 + 最后处理选项（并行）
	trackGroupTime("FinalTasksAndOptionProcessing", map[string]func(*sync.WaitGroup){
		"cpp2.WriteServiceRegisterInfoFile": cpp2.WriteServiceRegisterInfoFile,
		"_go2.GenerateDBResource":           _go2.GenerateDBResource,
		"_go2.GoRobotHandlerGenerator":      _go2.GoRobotHandlerGenerator,
		"_go2.GoRobotTotalHandlerGenerator": _go2.GoRobotTotalHandlerGenerator,
		"cpp2.CppPlayerDataLoadGenerator":   cpp2.CppPlayerDataLoadGenerator,
		"cpp2.CppGrpcCallClient":            cpp2.CppGrpcCallClient,
		"proto_tools_option.ProcessAllOptions": func(wg *sync.WaitGroup) {
			proto_tools_option.ProcessAllOptions(wg, internal.FdSet)
		},
	})

	// 打印所有统计信息
	printStats()

	// 打印总耗时
	logger.Global.Info("Total execution time", zap.Duration("elapsed", time.Since(start)))

	// 替换select{}，添加信号监听
	logger.Global.Info("程序已进入等待状态，可访问localhost:11111/debug/pprof分析，按Ctrl+C退出")
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)
	<-sigChan

	logger.Global.Info("程序开始优雅退出...")
}
