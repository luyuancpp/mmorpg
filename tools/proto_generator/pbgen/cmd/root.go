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
func waitWithTiming(wg *sync.WaitGroup, name string) {
	start := time.Now()
	wg.Wait()
	elapsed := time.Since(start)
	log.Printf("Wait [%s] took: %s", name, elapsed)
}

func main() {
	if err := _config.Load(); err != nil {
		log.Fatalf("配置初始化失败: %v", err)
	}

	// 后续使用 config.Global 访问配置
	log.Printf("配置加载成功，proto根目录: %s", _config.Global.Paths.OutputRoot)

	start := time.Now() // 记录开始时间

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
	wg.Wait()                           // 这里使用的是 wg，单独记录
	utilsWgElapsed := time.Since(start) // 注意：这里如果前面有其他操作，需要单独记录 start
	log.Printf("Wait [wg] took: %s", utilsWgElapsed)

	cpp2.GenerateAllEventHandlers(&wg)
	waitWithTiming(&wg, "Fifth wait (GenerateAllEventHandlers)")

	// 所有文件的proto读完以后
	cpp2.InitServiceId()
	waitWithTiming(&wg, "Sixth wait (InitServiceId)")

	cpp2.WriteServiceIdFile()
	waitWithTiming(&wg, "Seventh wait (WriteServiceIdFile)")

	cpp2.WriteMethodFile(&wg)
	cpp2.GeneratorHandler(&wg)
	waitWithTiming(&wg, "Eighth wait (WriteMethodFile/GeneratorHandler)")

	internal.GenerateServiceConstants(&wg)
	waitWithTiming(&wg, "Ninth wait (GenerateServiceConstants)")

	internal.WriteGoMessageId(&wg)
	waitWithTiming(&wg, "Tenth wait (WriteGoMessageId)")

	cpp2.WriteServiceRegisterInfoFile(&wg)
	_go2.GenerateDBResource(&wg)
	_go2.GoRobotHandlerGenerator(&wg)
	_go2.GoRobotTotalHandlerGenerator(&wg)
	cpp2.CppPlayerDataLoadGenerator(&wg)
	cpp2.CppGrpcCallClient(&wg)
	wg.Wait()                            // 这里又使用了 wg，单独记录
	utilsWgElapsed2 := time.Since(start) // 同样需要单独记录 start
	log.Printf("Wait [wg 2] took: %s", utilsWgElapsed2)

	// 打印总耗时
	log.Printf("Total execution time: %s\n", time.Since(start))
}
