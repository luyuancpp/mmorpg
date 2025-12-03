package main

import (
	"fmt"
	"log"
	"net/http"
	_ "net/http/pprof"
	"os"
	"pbgen/global_value"
	"pbgen/internal"
	_config "pbgen/internal/config"
	cpp2 "pbgen/internal/generator/cpp"
	_go_option "pbgen/internal/generator/cpp/options"
	_go2 "pbgen/internal/generator/go"
	_cpp_option "pbgen/internal/generator/go/options"
	"pbgen/internal/prototools"
	proto_tools_option "pbgen/internal/prototools/option"
	"sort"
	"sync"
	"time"
)

// WaitTimeRecord 存储等待耗时记录
type WaitTimeRecord struct {
	Name     string
	Duration time.Duration
}

var waitRecords []WaitTimeRecord
var taskRecords []WaitTimeRecord // 单独存储任务耗时

// 跟踪单个函数耗时的工具（保持不变）
func trackFuncTime(funcName string, f func()) {
	start := time.Now()
	f()
	elapsed := time.Since(start)
	taskRecords = append(taskRecords, WaitTimeRecord{
		Name:     funcName,
		Duration: elapsed,
	})
	log.Printf("Function [%s] took: %s", funcName, elapsed)
}

// 修改：使用全局传递的wg，不再创建分组内部wg
func trackGroupTime(groupName string, wg *sync.WaitGroup, tasks map[string]func(*sync.WaitGroup)) {
	start := time.Now()

	// 启动所有并行任务（使用全局传递的wg）
	for name, task := range tasks {
		wg.Add(1)
		go func(n string, t func(*sync.WaitGroup)) {
			defer wg.Done()
			trackFuncTime(n, func() { t(wg) }) // 传递全局wg
		}(name, task)
	}

	// 等待当前分组所有并行任务完成
	wg.Wait()
	elapsed := time.Since(start)
	waitRecords = append(waitRecords, WaitTimeRecord{
		Name:     "Group: " + groupName,
		Duration: elapsed,
	})
	log.Printf("Group [%s] total took: %s", groupName, elapsed)
}

// 修改：串行分组也使用全局传递的wg
func trackSerialGroupTime(groupName string, wg *sync.WaitGroup, tasks []struct {
	FuncName string
	Func     func(*sync.WaitGroup)
}) {
	start := time.Now()

	// 串行执行所有任务（使用全局wg）
	for _, task := range tasks {
		trackFuncTime(task.FuncName, func() {
			task.Func(wg)
			wg.Wait() // 等待当前任务完成
		})
	}

	// 记录分组总耗时
	elapsed := time.Since(start)
	waitRecords = append(waitRecords, WaitTimeRecord{
		Name:     "Group: " + groupName,
		Duration: elapsed,
	})
	log.Printf("Serial Group [%s] total took: %s", groupName, elapsed)
}

func MakeProjectDir() {
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

// 保留原有工具函数（如需单独使用等待耗时统计）
func waitWithTiming(wg *sync.WaitGroup, name string) time.Duration {
	start := time.Now()
	wg.Wait()
	elapsed := time.Since(start)

	waitRecords = append(waitRecords, WaitTimeRecord{
		Name:     name,
		Duration: elapsed,
	})

	log.Printf("Wait [%s] took: %s", name, elapsed)
	return elapsed
}

// 打印耗时统计信息（保持不变）
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

func main() {
	if err := _config.Load(); err != nil {
		log.Fatalf("配置初始化失败: %v", err)
	}

	log.Printf("配置加载成功，proto根目录: %s", _config.Global.Paths.OutputRoot)

	start := time.Now()

	go func() {
		log.Println(http.ListenAndServe("localhost:11111", nil))
	}()

	dir, err := os.Getwd()
	if err != nil {
		log.Fatal(err)
	}

	fmt.Println("Current working directory:", dir)

	// 仅声明一个全局WaitGroup，所有分组共用
	var wg sync.WaitGroup

	// 分组1：初始化项目目录（串行，无依赖）
	trackFuncTime("MakeProjectDir", MakeProjectDir)

	// 分组2：Proto文件处理（串行：先拷贝，再AddGoPackage）
	// 传递全局wg给串行分组函数
	trackSerialGroupTime("ProtoFilePreparation", &wg, []struct {
		FuncName string
		Func     func(*sync.WaitGroup)
	}{
		{FuncName: "prototools.CopyProtoToGenDir", Func: prototools.CopyProtoToGenDir},
		{FuncName: "_go2.AddGoPackageToProtoDir", Func: _go2.AddGoPackageToProtoDir},
	})

	// 分组3：GRPC生成与ServiceId读取（并行）
	// 传递全局wg给并行分组函数
	trackGroupTime("GRPCGeneration", &wg, map[string]func(*sync.WaitGroup){
		"cpp2.GenerateGameGrpc":  cpp2.GenerateGameGrpc,
		"_go2.GenerateGameGrpc":  _go2.GenerateGameGrpc,
		"cpp2.ReadServiceIdFile": cpp2.ReadServiceIdFile,
	})

	// 分组4：描述符生成（依赖分组2和3，使用全局wg）
	trackFuncTime("prototools.GenerateAllInOneDescriptor", func() {
		prototools.GenerateAllInOneDescriptor(&wg)
		wg.Wait()
	})

	// 分组5：服务信息读取（依赖分组4，使用全局wg）
	trackFuncTime("cpp2.ReadAllProtoFileServices", func() {
		cpp2.ReadAllProtoFileServices(&wg)
		wg.Wait()
	})

	// 分组6：编译相关（C++和Go并行处理）
	trackGroupTime("ProtobufCompilation", &wg, map[string]func(*sync.WaitGroup){
		"cpp2.BuildProtocCpp":        cpp2.BuildProtocCpp,
		"_go2.BuildGrpcServiceProto": _go2.BuildGrpcServiceProto,
	})

	// 分组7：工具类与事件处理器生成（可并行）
	trackGroupTime("UtilsAndEventHandlers", &wg, map[string]func(*sync.WaitGroup){
		"cpp2.GenNodeUtil":              cpp2.GenNodeUtil,
		"cpp2.GenerateAllEventHandlers": cpp2.GenerateAllEventHandlers,
	})

	// 分组8：ServiceId初始化与写入（串行依赖，无wg操作）
	trackFuncTime("cpp2.InitServiceId", cpp2.InitServiceId)
	trackFuncTime("cpp2.WriteServiceIdFile", cpp2.WriteServiceIdFile)

	// 分组9：方法文件与处理器生成（并行）
	trackGroupTime("MethodFilesGeneration", &wg, map[string]func(*sync.WaitGroup){
		"cpp2.WriteMethodFile":  cpp2.WriteMethodFile,
		"cpp2.GeneratorHandler": cpp2.GeneratorHandler,
	})

	// 分组10：常量与消息ID生成（并行）
	trackGroupTime("ConstantsAndMessageIds", &wg, map[string]func(*sync.WaitGroup){
		"internal.GenerateServiceConstants": internal.GenerateServiceConstants,
		"internal.WriteGoMessageId":         internal.WriteGoMessageId,
	})

	// 分组11：选项构建（并行）
	trackGroupTime("OptionBuilding", &wg, map[string]func(*sync.WaitGroup){
		"_go_option.BuildOption": func(wg *sync.WaitGroup) {
			// 内部启动goroutine时，使用局部计数避免干扰全局wg
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

	// 分组12：最终任务（并行）
	trackGroupTime("FinalTasks", &wg, map[string]func(*sync.WaitGroup){
		"cpp2.WriteServiceRegisterInfoFile": cpp2.WriteServiceRegisterInfoFile,
		"_go2.GenerateDBResource":           _go2.GenerateDBResource,
		"_go2.GoRobotHandlerGenerator":      _go2.GoRobotHandlerGenerator,
		"_go2.GoRobotTotalHandlerGenerator": _go2.GoRobotTotalHandlerGenerator,
		"cpp2.CppPlayerDataLoadGenerator":   cpp2.CppPlayerDataLoadGenerator,
		"cpp2.CppGrpcCallClient":            cpp2.CppGrpcCallClient,
	})

	// 最后处理选项（使用全局wg）
	trackFuncTime("proto_tools_option.ProcessAllOptions", func() {
		proto_tools_option.ProcessAllOptions(&wg, internal.FdSet)
		wg.Wait()
	})

	// 打印所有统计信息
	printStats()

	// 打印总耗时
	log.Printf("\nTotal execution time: %s\n", time.Since(start))
}
