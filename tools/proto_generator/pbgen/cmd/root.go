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

// trackFuncTime 接收全局wg指针，供任务使用（保持不变）
func trackFuncTime(funcName string, wg *sync.WaitGroup, f func(*sync.WaitGroup)) {
	start := time.Now()
	f(wg) // 传递全局wg给任务函数
	elapsed := time.Since(start)
	taskRecords = append(taskRecords, WaitTimeRecord{
		Name:     funcName,
		Duration: elapsed,
	})
	log.Printf("Function [%s] took: %s", funcName, elapsed)
}

// trackGroupTime 接收全局wg，并行任务均使用该wg（保持不变）
func trackGroupTime(groupName string, wg *sync.WaitGroup, tasks map[string]func(*sync.WaitGroup)) {
	start := time.Now()

	// 启动所有并行任务（共用全局wg）
	for name, task := range tasks {
		wg.Add(1)
		go func(n string, t func(*sync.WaitGroup)) {
			defer wg.Done()
			// 调用trackFuncTime，传递全局wg
			trackFuncTime(n, wg, func(w *sync.WaitGroup) {
				t(w) // 任务函数使用全局wg
			})
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
	// 重置wg，避免累计计数影响后续分组
	*wg = sync.WaitGroup{}
}

// trackSerialGroupTime 接收全局wg，串行任务均使用该wg（保持不变）
func trackSerialGroupTime(groupName string, wg *sync.WaitGroup, tasks []struct {
	FuncName string
	Func     func(*sync.WaitGroup)
}) {
	start := time.Now()

	// 串行执行所有任务（共用全局wg）
	for _, task := range tasks {
		trackFuncTime(task.FuncName, wg, func(w *sync.WaitGroup) {
			task.Func(w)          // 任务函数使用全局wg
			w.Wait()              // 等待当前串行任务完成
			*w = sync.WaitGroup{} // 重置wg
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

func MakeProjectDir(wg *sync.WaitGroup) {
	// 该函数无并发操作，wg仅为统一参数格式
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

	// 仅声明一个全局WaitGroup，所有函数统一传递该wg
	var wg sync.WaitGroup

	// 分组1：初始化项目目录（串行，无依赖）
	trackFuncTime("MakeProjectDir", &wg, MakeProjectDir)

	// 分组2：Proto文件处理（串行：先拷贝，再AddGoPackage）
	trackSerialGroupTime("ProtoFilePreparation", &wg, []struct {
		FuncName string
		Func     func(*sync.WaitGroup)
	}{
		{FuncName: "prototools.CopyProtoToGenDir", Func: prototools.CopyProtoToGenDir},
		{FuncName: "_go2.AddGoPackageToProtoDir", Func: _go2.AddGoPackageToProtoDir},
	})

	// 分组3：GRPC生成与ServiceId读取（并行）
	trackGroupTime("GRPCGeneration", &wg, map[string]func(*sync.WaitGroup){
		"cpp2.GenerateGameGrpc":  cpp2.GenerateGameGrpc,
		"_go2.GenerateGameGrpc":  _go2.GenerateGameGrpc,
		"cpp2.ReadServiceIdFile": cpp2.ReadServiceIdFile,
	})

	// 分组4：描述符生成（依赖分组2和3）
	trackFuncTime("prototools.GenerateAllInOneDescriptor", &wg, func(w *sync.WaitGroup) {
		prototools.GenerateAllInOneDescriptor(w)
		w.Wait()
		*w = sync.WaitGroup{} // 重置wg
	})

	// 分组5：服务信息读取（依赖分组4）
	trackFuncTime("cpp2.ReadAllProtoFileServices", &wg, func(w *sync.WaitGroup) {
		cpp2.ReadAllProtoFileServices(w)
		w.Wait()
		*w = sync.WaitGroup{} // 重置wg
	})

	// 合并分组：编译+工具类+事件处理器生成（所有任务并行执行）
	// 分组名称改为"CompilationAndUtilGeneration"，包含原6、7分组的所有任务
	trackGroupTime("CompilationAndUtilGeneration", &wg, map[string]func(*sync.WaitGroup){
		// 原分组6的任务
		"cpp2.BuildProtocCpp":        cpp2.BuildProtocCpp,
		"_go2.BuildGrpcServiceProto": _go2.BuildGrpcServiceProto,
		// 原分组7的任务
		"cpp2.GenNodeUtil":              cpp2.GenNodeUtil,
		"cpp2.GenerateAllEventHandlers": cpp2.GenerateAllEventHandlers,
	})

	// 分组6：ServiceId初始化与写入（串行依赖）
	trackFuncTime("cpp2.InitServiceId", &wg, func(w *sync.WaitGroup) {
		cpp2.InitServiceId() // 无并发操作，仅为统一格式传递wg
	})
	trackFuncTime("cpp2.WriteServiceIdFile", &wg, func(w *sync.WaitGroup) {
		cpp2.WriteServiceIdFile() // 无并发操作，仅为统一格式传递wg
	})

	// 分组7：方法文件与处理器生成（并行）
	trackGroupTime("MethodFilesGeneration", &wg, map[string]func(*sync.WaitGroup){
		"cpp2.WriteMethodFile":  cpp2.WriteMethodFile,
		"cpp2.GeneratorHandler": cpp2.GeneratorHandler,
	})

	// 分组8：常量与消息ID生成（并行）
	trackGroupTime("ConstantsAndMessageIds", &wg, map[string]func(*sync.WaitGroup){
		"internal.GenerateServiceConstants": internal.GenerateServiceConstants,
		"internal.WriteGoMessageId":         internal.WriteGoMessageId,
	})

	// 分组9：选项构建（并行）
	trackGroupTime("OptionBuilding", &wg, map[string]func(*sync.WaitGroup){
		"_go_option.BuildOption": func(w *sync.WaitGroup) {
			// 内部goroutine使用局部wg，避免干扰全局计数
			localWg := sync.WaitGroup{}
			localWg.Add(1)
			go func() { defer localWg.Done(); _go_option.BuildOption() }()
			localWg.Wait()
		},
		"_cpp_option.BuildOption": func(w *sync.WaitGroup) {
			localWg := sync.WaitGroup{}
			localWg.Add(1)
			go func() { defer localWg.Done(); _cpp_option.BuildOption() }()
			localWg.Wait()
		},
	})

	// 分组10：最终任务（并行）
	trackGroupTime("FinalTasks", &wg, map[string]func(*sync.WaitGroup){
		"cpp2.WriteServiceRegisterInfoFile": cpp2.WriteServiceRegisterInfoFile,
		"_go2.GenerateDBResource":           _go2.GenerateDBResource,
		"_go2.GoRobotHandlerGenerator":      _go2.GoRobotHandlerGenerator,
		"_go2.GoRobotTotalHandlerGenerator": _go2.GoRobotTotalHandlerGenerator,
		"cpp2.CppPlayerDataLoadGenerator":   cpp2.CppPlayerDataLoadGenerator,
		"cpp2.CppGrpcCallClient":            cpp2.CppGrpcCallClient,
	})

	// 最后处理选项
	trackFuncTime("proto_tools_option.ProcessAllOptions", &wg, func(w *sync.WaitGroup) {
		proto_tools_option.ProcessAllOptions(w, internal.FdSet)
		w.Wait()
		*w = sync.WaitGroup{}
	})

	// 打印所有统计信息
	printStats()

	// 打印总耗时
	log.Printf("\nTotal execution time: %s\n", time.Since(start))
}
