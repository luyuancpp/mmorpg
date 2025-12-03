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
var taskRecords []WaitTimeRecord  // 单独存储每个函数的耗时
var groupRecords []WaitTimeRecord // 单独存储分组总耗时

// 新增：仅跟踪单个函数的耗时（不改变函数执行逻辑）
func trackSingleFunc(name string, f func()) {
	start := time.Now()
	f() // 直接执行函数，不改变原有逻辑
	elapsed := time.Since(start)
	taskRecords = append(taskRecords, WaitTimeRecord{
		Name:     name,
		Duration: elapsed,
	})
	log.Printf("Function [%s] took: %s", name, elapsed)
}

// 新增：跟踪分组总耗时（仅计时，不改变任务执行顺序）
func trackGroupStart(groupName string) time.Time {
	log.Printf("=== Start Group: %s ===", groupName)
	return time.Now()
}

// 新增：结束分组计时
func trackGroupEnd(groupName string, start time.Time) {
	elapsed := time.Since(start)
	groupRecords = append(groupRecords, WaitTimeRecord{
		Name:     "Group: " + groupName,
		Duration: elapsed,
	})
	log.Printf("=== Group [%s] total took: %s ===\n", groupName, elapsed)
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

// 原有的等待耗时工具函数（保持不变）
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

// 优化统计打印：同时展示分组、等待、单个函数耗时
func printStats() {
	fmt.Println("\n======================================")
	fmt.Println("=== Group Total Time Statistics ===")
	fmt.Println("======================================")
	if len(groupRecords) > 0 {
		sortedGroups := make([]WaitTimeRecord, len(groupRecords))
		copy(sortedGroups, groupRecords)
		sort.Slice(sortedGroups, func(i, j int) bool {
			return sortedGroups[i].Duration > sortedGroups[j].Duration
		})

		for i, record := range sortedGroups {
			prefix := "  "
			if i == 0 {
				prefix = "\033[31m→ "
			}
			fmt.Printf("%s%d. %-50s %s\033[0m\n", prefix, i+1, record.Name, record.Duration)
		}
	}

	fmt.Println("\n======================================")
	fmt.Println("=== Wait Time Statistics ===")
	fmt.Println("======================================")
	if len(waitRecords) > 0 {
		sortedWaits := make([]WaitTimeRecord, len(waitRecords))
		copy(sortedWaits, waitRecords)
		sort.Slice(sortedWaits, func(i, j int) bool {
			return sortedWaits[i].Duration > sortedWaits[j].Duration
		})

		for i, record := range sortedWaits {
			prefix := "  "
			if i == 0 {
				prefix = "\033[34m→ "
			}
			fmt.Printf("%s%d. %-50s %s\033[0m\n", prefix, i+1, record.Name, record.Duration)
		}
	}

	fmt.Println("\n======================================")
	fmt.Println("=== Individual Function Time Statistics ===")
	fmt.Println("======================================")
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
			fmt.Printf("%s%d. %-50s %s\033[0m\n", prefix, i+1, record.Name, record.Duration)
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

	// 分组1：项目目录初始化
	group1Start := trackGroupStart("ProjectDirInit")
	trackSingleFunc("MakeProjectDir", MakeProjectDir)
	trackGroupEnd("ProjectDirInit", group1Start)

	var wg sync.WaitGroup

	// 分组2：Proto文件复制
	group2Start := trackGroupStart("ProtoCopy")
	trackSingleFunc("prototools.CopyProtoToGenDir", func() {
		prototools.CopyProtoToGenDir(&wg)
	})
	waitWithTiming(&wg, "First wait (CopyProto)")
	trackGroupEnd("ProtoCopy", group2Start)

	// 分组3：Go包处理
	group3Start := trackGroupStart("GoPackageProcessing")
	trackSingleFunc("_go2.AddGoPackageToProtoDir", func() {
		_go2.AddGoPackageToProtoDir(&wg)
	})
	waitWithTiming(&wg, "Third wait (AddGoPackageToProtoDir)")
	trackGroupEnd("GoPackageProcessing", group3Start)

	// 分组4：GRPC生成与ServiceId读取
	group4Start := trackGroupStart("GRPCGenerationAndServiceId")
	trackSingleFunc("cpp2.GenerateGameGrpc", func() {
		cpp2.GenerateGameGrpc(&wg)
	})
	trackSingleFunc("_go2.GenerateGameGrpc", func() {
		_go2.GenerateGameGrpc(&wg)
	})
	trackSingleFunc("cpp2.ReadServiceIdFile", func() {
		cpp2.ReadServiceIdFile(&wg)
	})
	waitWithTiming(&wg, "First wait (GenerateGameGrpc/ReadServiceIdFile)")
	trackGroupEnd("GRPCGenerationAndServiceId", group4Start)

	// 分组5：描述符生成
	group5Start := trackGroupStart("DescriptorGeneration")
	trackSingleFunc("prototools.GenerateAllInOneDescriptor", func() {
		prototools.GenerateAllInOneDescriptor(&wg)
	})
	waitWithTiming(&wg, "Second wait (GenerateAllInOneDescriptor)")
	trackGroupEnd("DescriptorGeneration", group5Start)

	// 分组6：服务信息读取
	group6Start := trackGroupStart("ServiceInfoReading")
	trackSingleFunc("cpp2.ReadAllProtoFileServices", func() {
		cpp2.ReadAllProtoFileServices(&wg)
	})
	waitWithTiming(&wg, "Third wait (AddGoPackageToProtoDir)")
	trackGroupEnd("ServiceInfoReading", group6Start)

	// 分组7：Protobuf编译
	group7Start := trackGroupStart("ProtobufCompilation")
	trackSingleFunc("cpp2.BuildProtocCpp", func() {
		cpp2.BuildProtocCpp(&wg)
	})
	trackSingleFunc("_go2.BuildGrpcServiceProto", func() {
		_go2.BuildGrpcServiceProto(&wg)
	})
	waitWithTiming(&wg, "Fourth wait (BuildProtocCpp/BuildGrpcServiceProto)")
	trackGroupEnd("ProtobufCompilation", group7Start)

	// 分组8：工具类生成
	group8Start := trackGroupStart("UtilGeneration")
	trackSingleFunc("cpp2.GenNodeUtil", func() {
		cpp2.GenNodeUtil(&wg)
	})
	waitWithTiming(&wg, "Fifth wait (GenNodeUtil)")
	trackGroupEnd("UtilGeneration", group8Start)

	// 分组9：事件处理器生成
	group9Start := trackGroupStart("EventHandlerGeneration")
	trackSingleFunc("cpp2.GenerateAllEventHandlers", func() {
		cpp2.GenerateAllEventHandlers(&wg)
	})
	waitWithTiming(&wg, "Sixth wait (GenerateAllEventHandlers)")
	trackGroupEnd("EventHandlerGeneration", group9Start)

	// 分组10：ServiceId初始化与写入
	group10Start := trackGroupStart("ServiceIdProcessing")
	trackSingleFunc("cpp2.InitServiceId", cpp2.InitServiceId)
	waitWithTiming(&wg, "Seventh wait (InitServiceId)")
	trackSingleFunc("cpp2.WriteServiceIdFile", cpp2.WriteServiceIdFile)
	waitWithTiming(&wg, "Eighth wait (WriteServiceIdFile)")
	trackGroupEnd("ServiceIdProcessing", group10Start)

	// 分组11：方法文件与处理器生成
	group11Start := trackGroupStart("MethodAndHandlerGeneration")
	trackSingleFunc("cpp2.WriteMethodFile", func() {
		cpp2.WriteMethodFile(&wg)
	})
	trackSingleFunc("cpp2.GeneratorHandler", func() {
		cpp2.GeneratorHandler(&wg)
	})
	waitWithTiming(&wg, "Ninth wait (WriteMethodFile/GeneratorHandler)")
	trackGroupEnd("MethodAndHandlerGeneration", group11Start)

	// 分组12：常量与消息ID生成
	group12Start := trackGroupStart("ConstantsAndMessageIdGeneration")
	trackSingleFunc("internal.GenerateServiceConstants", func() {
		internal.GenerateServiceConstants(&wg)
	})
	waitWithTiming(&wg, "Tenth wait (GenerateServiceConstants)")
	trackSingleFunc("internal.WriteGoMessageId", func() {
		internal.WriteGoMessageId(&wg)
	})
	waitWithTiming(&wg, "Eleventh wait (WriteGoMessageId)")
	trackGroupEnd("ConstantsAndMessageIdGeneration", group12Start)

	// 分组13：选项构建
	group13Start := trackGroupStart("OptionBuilding")
	trackSingleFunc("_go_option.BuildOption", _go_option.BuildOption)
	trackSingleFunc("_cpp_option.BuildOption", _cpp_option.BuildOption)
	trackGroupEnd("OptionBuilding", group13Start)

	// 分组14：最终任务
	group14Start := trackGroupStart("FinalTasks")
	trackSingleFunc("cpp2.WriteServiceRegisterInfoFile", func() {
		cpp2.WriteServiceRegisterInfoFile(&wg)
	})
	trackSingleFunc("_go2.GenerateDBResource", func() {
		_go2.GenerateDBResource(&wg)
	})
	trackSingleFunc("_go2.GoRobotHandlerGenerator", func() {
		_go2.GoRobotHandlerGenerator(&wg)
	})
	trackSingleFunc("_go2.GoRobotTotalHandlerGenerator", func() {
		_go2.GoRobotTotalHandlerGenerator(&wg)
	})
	trackSingleFunc("cpp2.CppPlayerDataLoadGenerator", func() {
		cpp2.CppPlayerDataLoadGenerator(&wg)
	})
	trackSingleFunc("cpp2.CppGrpcCallClient", func() {
		cpp2.CppGrpcCallClient(&wg)
	})

	// 跟踪整体等待时间（原有逻辑不变）
	startFinal := time.Now()
	wg.Wait()
	elapsedFinal := time.Since(startFinal)
	log.Printf("Final tasks total wait time: %s", elapsedFinal)
	trackGroupEnd("FinalTasks", group14Start)

	// 分组15：选项处理
	group15Start := trackGroupStart("FinalOptionProcessing")
	trackSingleFunc("proto_tools_option.ProcessAllOptions", func() {
		proto_tools_option.ProcessAllOptions(&wg, internal.FdSet)
	})
	wg.Wait()
	trackGroupEnd("FinalOptionProcessing", group15Start)

	// 打印所有统计信息
	printStats()

	// 打印总耗时（原有逻辑不变）
	log.Printf("\nTotal execution time: %s\n", time.Since(start))
}
