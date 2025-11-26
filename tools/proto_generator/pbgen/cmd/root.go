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
	"pbgen/internal/utils"
	"time"
)

func MakeProjectDir() {
	os.MkdirAll(config.GeneratedOutputDirectory, os.FileMode(0777))
	os.MkdirAll(config.PbcLuaDirectory, os.FileMode(0777))
	os.MkdirAll(config.ServiceInfoDirectory, os.FileMode(0777))
	os.MkdirAll(config.CppGenGrpcDirectory, os.FileMode(0777))
	os.MkdirAll(config.PbDescDirectory, os.FileMode(0777))
	os.MkdirAll(config.TempFileGenerationDir, os.FileMode(0777))
	os.MkdirAll(config.PbcTempDirectory, os.FileMode(0777))
	os.MkdirAll(config.GrpcTempDirectory, os.FileMode(0777))
	os.MkdirAll(config.GoGeneratorDirectory, os.FileMode(0777))
	os.MkdirAll(config.ProjectGeneratedPath, os.FileMode(0777))
	os.MkdirAll(config.TableGeneratorPath, os.FileMode(0777))
	os.RemoveAll(config.GeneratorProtoDirectory)
	os.MkdirAll(config.ProtoNormalPackageDirectory, os.FileMode(0777))

	for i := 0; i < len(config.ProtoDirectoryNames); i++ {
		config.ProtoDirs = append(config.ProtoDirs, config.ProtoDir+config.ProtoDirectoryNames[i])
	}
}

func main() {
	if err := _config.Load(); err != nil {
		log.Fatalf("配置初始化失败: %v", err)
	}

	// 后续使用 config.Global 访问配置
	log.Printf("配置加载成功，proto根目录: %s", _config.Global.Paths.ProtoRoot)

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
	err = cpp2.GenerateGameGrpc()
	if err != nil {
		log.Fatal(err)
		return
	}
	err = _go2.GenerateGameGrpc()
	if err != nil {
		log.Fatal(err)
		return
	}
	proto.CopyProtoToGenDir()

	// 开始读所有的proto文件
	cpp2.ReadServiceIdFile()
	utils.Wg.Wait()

	_go2.AddGoPackageToProtoDir()
	utils.Wg.Wait()

	proto.GenerateAllInOneDescriptor()
	utils.Wg.Wait()
	cpp2.ReadAllProtoFileServices()
	utils.Wg.Wait()
	cpp2.BuildProtocCpp()
	_go2.BuildGrpcServiceProto()
	utils.Wg.Wait()

	cpp2.GenNodeUtil()
	utils.Wg.Wait()

	cpp2.GenerateAllEventHandlers()
	utils.Wg.Wait()
	// 所有文件的proto读完以后
	cpp2.InitServiceId()
	utils.Wg.Wait()

	cpp2.WriteServiceIdFile()
	utils.Wg.Wait()

	cpp2.WriteMethodFile()
	cpp2.GeneratorHandler()
	utils.Wg.Wait()

	internal.GenerateServiceConstants()
	utils.Wg.Wait()
	// 所有service初始化完以后
	internal.WriteGoMessageId()
	utils.Wg.Wait()

	cpp2.WriteServiceRegisterInfoFile()
	_go2.GenerateDBResource()
	utils.Wg.Wait()

	_go2.GoRobotHandlerGenerator()
	utils.Wg.Wait()
	_go2.GoRobotTotalHandlerGenerator()
	utils.Wg.Wait()
	cpp2.CppPlayerDataLoadGenerator()
	utils.Wg.Wait()
	cpp2.CppGrpcCallClient()
	utils.Wg.Wait()

	// 打印总耗时
	log.Printf("Total execution time: %s\n", time.Since(start))

}
