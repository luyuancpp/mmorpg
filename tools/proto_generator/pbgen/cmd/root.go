package main

import (
	"fmt"
	"log"
	"net/http"
	_ "net/http/pprof"
	"os"
	"pbgen/config"
	"pbgen/internal"
	_config "pbgen/internal/config"
	cpp2 "pbgen/internal/generator/cpp"
	_go2 "pbgen/internal/generator/go"
	"pbgen/internal/proto"
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

func MakeProjectDir() {
	os.MkdirAll(_config.Global.Paths.GeneratedDir, os.FileMode(0777))
	os.MkdirAll(config.ServiceInfoDirectory, os.FileMode(0777))
	os.MkdirAll(config.CppGenGrpcDirectory, os.FileMode(0777))
	os.MkdirAll(config.PbDescDirectory, os.FileMode(0777))
	os.MkdirAll(_config.Global.Paths.TempFileGenDir, os.FileMode(0775))
	os.MkdirAll(config.PbcTempDirectory, os.FileMode(0777))
	os.MkdirAll(config.GrpcTempDirectory, os.FileMode(0777))
	os.MkdirAll(config.GoGeneratorDirectory, os.FileMode(0777))
	os.MkdirAll(_config.Global.Paths.TableGeneratorDir, os.FileMode(0777))

	for i := 0; i < len(config.ProtoDirectoryNames); i++ {
		config.ProtoDirs = append(config.ProtoDirs, _config.Global.Paths.ProtoDir+config.ProtoDirectoryNames[i])
	}
}

// 记录等待耗时的工具函数
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

// 记录任务执行耗时
func trackTaskTime(taskName string, taskFunc func(*sync.WaitGroup)) {
	var taskWg sync.WaitGroup
	start := time.Now()

	// 执行任务
	taskFunc(&taskWg)

	// 等待任务完成并记录时间
	taskWg.Wait()
	elapsed := time.Since(start)

	taskRecords = append(taskRecords, WaitTimeRecord{
		Name:     taskName,
		Duration: elapsed,
	})

	log.Printf("Task [%s] took: %s", taskName, elapsed)
}

// 打印耗时统计信息
func printStats() {
	fmt.Println("\n=== Wait Time Statistics ===")
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

	fmt.Println("\n=== Task Time Statistics ===")
	if len(taskRecords) > 0 {
		sortedTasks := make([]WaitTimeRecord, len(taskRecords))
		copy(sortedTasks, taskRecords)
		sort.Slice(sortedTasks, func(i, j int) bool {
			return sortedTasks[i].Duration > sortedTasks[j].Duration
		})

		for i, record := range sortedTasks {
			prefix := "  "
			if i == 0 {
				prefix = "\033[33m→ " // 黄色标记最长任务
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

	MakeProjectDir()

	var wg sync.WaitGroup
	cpp2.GenerateGameGrpc(&wg)
	_go2.GenerateGameGrpc(&wg)
	proto.CopyProtoToGenDir(&wg)
	cpp2.ReadServiceIdFile(&wg)
	waitWithTiming(&wg, "First wait (GenerateGameGrpc/CopyProto/ReadServiceIdFile)")

	proto.GenerateAllInOneDescriptor(&wg)
	waitWithTiming(&wg, "Second wait (GenerateAllInOneDescriptor)")

	_go2.AddGoPackageToProtoDir(&wg)
	cpp2.ReadAllProtoFileServices(&wg)
	waitWithTiming(&wg, "Third wait (AddGoPackageToProtoDir/ReadAllProtoFileServices)")

	cpp2.BuildProtocCpp(&wg)
	_go2.BuildGrpcServiceProto(&wg)
	waitWithTiming(&wg, "Fourth wait (BuildProtocCpp/BuildGrpcServiceProto)")

	cpp2.GenNodeUtil(&wg)
	waitWithTiming(&wg, "Fifth wait (GenNodeUtil)")

	cpp2.GenerateAllEventHandlers(&wg)
	waitWithTiming(&wg, "Sixth wait (GenerateAllEventHandlers)")

	cpp2.InitServiceId()
	waitWithTiming(&wg, "Seventh wait (InitServiceId)")

	cpp2.WriteServiceIdFile()
	waitWithTiming(&wg, "Eighth wait (WriteServiceIdFile)")

	cpp2.WriteMethodFile(&wg)
	cpp2.GeneratorHandler(&wg)
	waitWithTiming(&wg, "Ninth wait (WriteMethodFile/GeneratorHandler)")

	internal.GenerateServiceConstants(&wg)
	waitWithTiming(&wg, "Tenth wait (GenerateServiceConstants)")

	internal.WriteGoMessageId(&wg)
	waitWithTiming(&wg, "Eleventh wait (WriteGoMessageId)")

	// 单独跟踪每个耗时任务

	// 逐个跟踪任务
	trackTaskTime("WriteServiceRegisterInfoFile", func(wg *sync.WaitGroup) {
		cpp2.WriteServiceRegisterInfoFile(wg)
	})

	trackTaskTime("GenerateDBResource", func(wg *sync.WaitGroup) {
		_go2.GenerateDBResource(wg)
	})

	trackTaskTime("GoRobotHandlerGenerator", func(wg *sync.WaitGroup) {
		_go2.GoRobotHandlerGenerator(wg)
	})

	trackTaskTime("GoRobotTotalHandlerGenerator", func(wg *sync.WaitGroup) {
		_go2.GoRobotTotalHandlerGenerator(wg)
	})

	trackTaskTime("CppPlayerDataLoadGenerator", func(wg *sync.WaitGroup) {
		cpp2.CppPlayerDataLoadGenerator(wg)
	})

	trackTaskTime("CppGrpcCallClient", func(wg *sync.WaitGroup) {
		cpp2.CppGrpcCallClient(wg)
	})

	// 或者如果你想并行执行这些任务并整体等待：
	/*
		cpp2.WriteServiceRegisterInfoFile(&finalWg)
		_go2.GenerateDBResource(&finalWg)
		_go2.GoRobotHandlerGenerator(&finalWg)
		_go2.GoRobotTotalHandlerGenerator(&finalWg)
		cpp2.CppPlayerDataLoadGenerator(&finalWg)
		cpp2.CppGrpcCallClient(&finalWg)

		// 跟踪整体等待时间
		startFinal := time.Now()
		finalWg.Wait()
		elapsedFinal := time.Since(startFinal)
		log.Printf("Final tasks total wait time: %s", elapsedFinal)
	*/

	// 打印所有统计信息
	printStats()

	// 打印总耗时
	log.Printf("\nTotal execution time: %s\n", time.Since(start))
}
