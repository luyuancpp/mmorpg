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
	"sync"
	"time"
)

func MakeProjectDir() {
	os.MkdirAll(_config.Global.Paths.GeneratedDir, os.FileMode(0777))
	os.MkdirAll(config.ServiceInfoDirectory, os.FileMode(0777))
	os.MkdirAll(config.CppGenGrpcDirectory, os.FileMode(0777))
	os.MkdirAll(config.PbDescDirectory, os.FileMode(0777))
	os.MkdirAll(_config.Global.Paths.TempFileGenDir, os.FileMode(0775))
	os.MkdirAll(config.PbcTempDirectory, os.FileMode(0777))
	os.MkdirAll(config.GrpcTempDirectory, os.FileMode(0777))
	os.MkdirAll(config.GoGeneratorDirectory, os.FileMode(0777))
	os.MkdirAll(config.ProjectGeneratedPath, os.FileMode(0777))
	os.MkdirAll(config.TableGeneratorPath, os.FileMode(0777))

	for i := 0; i < len(config.ProtoDirectoryNames); i++ {
		config.ProtoDirs = append(config.ProtoDirs, _config.Global.Paths.ProtoDir+config.ProtoDirectoryNames[i])
	}
}

// 记录等待耗时的工具函数
func waitWithTiming(wg *sync.WaitGroup, name string) time.Duration {
	start := time.Now()
	wg.Wait()
	elapsed := time.Since(start)
	log.Printf("Wait [%s] took: %s", name, elapsed)
	return elapsed
}

func main() {
	if err := _config.Load(); err != nil {
		log.Fatalf("配置初始化失败: %v", err)
	}

	// 后续使用 config.Global 访问配置
	log.Printf("配置加载成功，proto根目录: %s", _config.Global.Paths.OutputRoot)

	start := time.Now() // 记录开始时间

	// 用于跟踪最长耗时的等待
	var maxDuration time.Duration
	var maxName string

	go func() {
		log.Println(http.ListenAndServe("localhost:11111", nil)) // 启动 pprof HTTP 服务
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
	elapsed := waitWithTiming(&wg, "First wait (GenerateGameGrpc/CopyProto/ReadServiceIdFile)")
	if elapsed > maxDuration {
		maxDuration = elapsed
		maxName = "First wait"
	}

	proto.GenerateAllInOneDescriptor(&wg)
	elapsed = waitWithTiming(&wg, "Second wait (GenerateAllInOneDescriptor)")
	if elapsed > maxDuration {
		maxDuration = elapsed
		maxName = "Second wait"
	}

	_go2.AddGoPackageToProtoDir(&wg)
	cpp2.ReadAllProtoFileServices(&wg)
	elapsed = waitWithTiming(&wg, "Third wait (AddGoPackageToProtoDir/ReadAllProtoFileServices)")
	if elapsed > maxDuration {
		maxDuration = elapsed
		maxName = "Third wait"
	}

	cpp2.BuildProtocCpp(&wg)
	_go2.BuildGrpcServiceProto(&wg)
	elapsed = waitWithTiming(&wg, "Fourth wait (BuildProtocCpp/BuildGrpcServiceProto)")
	if elapsed > maxDuration {
		maxDuration = elapsed
		maxName = "Fourth wait"
	}

	cpp2.GenNodeUtil(&wg)
	startWg := time.Now()
	wg.Wait()
	elapsed = time.Since(startWg)
	log.Printf("Wait [Fifth wait (GenNodeUtil)] took: %s", elapsed)
	if elapsed > maxDuration {
		maxDuration = elapsed
		maxName = "Fifth wait (GenNodeUtil)"
	}

	cpp2.GenerateAllEventHandlers(&wg)
	elapsed = waitWithTiming(&wg, "Sixth wait (GenerateAllEventHandlers)")
	if elapsed > maxDuration {
		maxDuration = elapsed
		maxName = "Sixth wait"
	}

	// 所有文件的proto读完以后
	cpp2.InitServiceId()
	elapsed = waitWithTiming(&wg, "Seventh wait (InitServiceId)")
	if elapsed > maxDuration {
		maxDuration = elapsed
		maxName = "Seventh wait"
	}

	cpp2.WriteServiceIdFile()
	elapsed = waitWithTiming(&wg, "Eighth wait (WriteServiceIdFile)")
	if elapsed > maxDuration {
		maxDuration = elapsed
		maxName = "Eighth wait"
	}

	cpp2.WriteMethodFile(&wg)
	cpp2.GeneratorHandler(&wg)
	elapsed = waitWithTiming(&wg, "Ninth wait (WriteMethodFile/GeneratorHandler)")
	if elapsed > maxDuration {
		maxDuration = elapsed
		maxName = "Ninth wait"
	}

	internal.GenerateServiceConstants(&wg)
	elapsed = waitWithTiming(&wg, "Tenth wait (GenerateServiceConstants)")
	if elapsed > maxDuration {
		maxDuration = elapsed
		maxName = "Tenth wait"
	}

	internal.WriteGoMessageId(&wg)
	elapsed = waitWithTiming(&wg, "Eleventh wait (WriteGoMessageId)")
	if elapsed > maxDuration {
		maxDuration = elapsed
		maxName = "Eleventh wait"
	}

	cpp2.WriteServiceRegisterInfoFile(&wg)
	_go2.GenerateDBResource(&wg)
	_go2.GoRobotHandlerGenerator(&wg)
	_go2.GoRobotTotalHandlerGenerator(&wg)
	cpp2.CppPlayerDataLoadGenerator(&wg)
	cpp2.CppGrpcCallClient(&wg)
	startWg = time.Now()
	wg.Wait()
	elapsed = time.Since(startWg)
	log.Printf("Wait [Twelfth wait (Final tasks)] took: %s", elapsed)
	if elapsed > maxDuration {
		maxDuration = elapsed
		maxName = "Twelfth wait (Final tasks)"
	}

	// 打印总耗时和最长耗时的等待
	log.Printf("Total execution time: %s\n", time.Since(start))
	log.Printf("LONGEST WAIT: [%s] with %s", maxName, maxDuration)
}
