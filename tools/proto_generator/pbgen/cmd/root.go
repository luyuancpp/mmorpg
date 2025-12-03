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

// 新增：跟踪单个函数耗时的工具
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

// 新增：跟踪分组内并行任务的耗时
func trackGroupTime(groupName string, tasks map[string]func(*sync.WaitGroup)) {
	var groupWg sync.WaitGroup
	start := time.Now()

	// 启动所有并行任务
	for name, task := range tasks {
		groupWg.Add(1)
		go func(n string, t func(*sync.WaitGroup)) {
			defer groupWg.Done()
			trackFuncTime(n, func() { t(&sync.WaitGroup{}) }) // 每个任务独立Wg避免干扰
		}(name, task)
	}

	// 等待分组完成
	groupWg.Wait()
	elapsed := time.Since(start)
	waitRecords = append(waitRecords, WaitTimeRecord{
		Name:     "Group: " + groupName,
		Duration: elapsed,
	})
	log.Printf("Group [%s] total took: %s", groupName, elapsed)
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

// 记录等待耗时的工具函数（保持不变）
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

	// 分组1：初始化项目目录（串行，无依赖）
	trackFuncTime("MakeProjectDir", MakeProjectDir)

	// 分组2：Proto文件处理（复制和Go包处理，可并行）
	trackGroupTime("ProtoFilePreparation", map[string]func(*sync.WaitGroup){
		"prototools.CopyProtoToGenDir": _go2.AddGoPackageToProtoDir,
		"_go2.AddGoPackageToProtoDir":  prototools.CopyProtoToGenDir,
	})

	// 分组3：GRPC生成与ServiceId读取（可并行）
	trackGroupTime("GRPCGeneration", map[string]func(*sync.WaitGroup){
		"cpp2.GenerateGameGrpc":  cpp2.GenerateGameGrpc,
		"_go2.GenerateGameGrpc":  _go2.GenerateGameGrpc,
		"cpp2.ReadServiceIdFile": cpp2.ReadServiceIdFile,
	})

	// 分组4：描述符生成（依赖分组2和3的结果）
	var wg4 sync.WaitGroup
	trackFuncTime("prototools.GenerateAllInOneDescriptor", func() {
		prototools.GenerateAllInOneDescriptor(&wg4)
		wg4.Wait()
	})

	// 分组5：服务信息读取（依赖分组4）
	var wg5 sync.WaitGroup
	trackFuncTime("cpp2.ReadAllProtoFileServices", func() {
		cpp2.ReadAllProtoFileServices(&wg5)
		wg5.Wait()
	})

	// 分组6：编译相关（C++和Go并行处理）
	trackGroupTime("ProtobufCompilation", map[string]func(*sync.WaitGroup){
		"cpp2.BuildProtocCpp":        cpp2.BuildProtocCpp,
		"_go2.BuildGrpcServiceProto": _go2.BuildGrpcServiceProto,
	})

	// 分组7：工具类与事件处理器生成（可并行）
	trackGroupTime("UtilsAndEventHandlers", map[string]func(*sync.WaitGroup){
		"cpp2.GenNodeUtil":              cpp2.GenNodeUtil,
		"cpp2.GenerateAllEventHandlers": cpp2.GenerateAllEventHandlers,
	})

	// 分组8：ServiceId初始化与写入（串行依赖）
	trackFuncTime("cpp2.InitServiceId", cpp2.InitServiceId)
	trackFuncTime("cpp2.WriteServiceIdFile", cpp2.WriteServiceIdFile)

	// 分组9：方法文件与处理器生成（并行）
	trackGroupTime("MethodFilesGeneration", map[string]func(*sync.WaitGroup){
		"cpp2.WriteMethodFile":  cpp2.WriteMethodFile,
		"cpp2.GeneratorHandler": cpp2.GeneratorHandler,
	})

	// 分组10：常量与消息ID生成（并行）
	trackGroupTime("ConstantsAndMessageIds", map[string]func(*sync.WaitGroup){
		"internal.GenerateServiceConstants": internal.GenerateServiceConstants,
		"internal.WriteGoMessageId":         internal.WriteGoMessageId,
	})

	// 分组11：选项构建（并行）
	trackGroupTime("OptionBuilding", map[string]func(*sync.WaitGroup){
		"_go_option.BuildOption":  func(wg *sync.WaitGroup) { wg.Add(1); go func() { defer wg.Done(); _go_option.BuildOption() }() },
		"_cpp_option.BuildOption": func(wg *sync.WaitGroup) { wg.Add(1); go func() { defer wg.Done(); _cpp_option.BuildOption() }() },
	})

	// 分组12：最终任务（并行）
	trackGroupTime("FinalTasks", map[string]func(*sync.WaitGroup){
		"cpp2.WriteServiceRegisterInfoFile": cpp2.WriteServiceRegisterInfoFile,
		"_go2.GenerateDBResource":           _go2.GenerateDBResource,
		"_go2.GoRobotHandlerGenerator":      _go2.GoRobotHandlerGenerator,
		"_go2.GoRobotTotalHandlerGenerator": _go2.GoRobotTotalHandlerGenerator,
		"cpp2.CppPlayerDataLoadGenerator":   cpp2.CppPlayerDataLoadGenerator,
		"cpp2.CppGrpcCallClient":            cpp2.CppGrpcCallClient,
	})

	// 最后处理选项
	var wgFinal sync.WaitGroup
	trackFuncTime("proto_tools_option.ProcessAllOptions", func() {
		proto_tools_option.ProcessAllOptions(&wgFinal, internal.FdSet)
		wgFinal.Wait()
	})

	// 打印所有统计信息
	printStats()

	// 打印总耗时
	log.Printf("\nTotal execution time: %s\n", time.Since(start))
}
