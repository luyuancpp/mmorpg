package main

import (
	"fmt"
	"log"
	"net/http"
	_ "net/http/pprof"
	"os"
	"pbgen/config"
	"pbgen/generator/cpp"
	"pbgen/generator/go"
	"pbgen/internal"
	"pbgen/internal/database"
	"pbgen/utils"
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
	err = cpp.GenerateGameGrpc()
	if err != nil {
		log.Fatal(err)
		return
	}
	err = _go.GenerateGameGrpc()
	if err != nil {
		log.Fatal(err)
		return
	}
	internal.CopyProtoToGenDir()

	// 开始读所有的proto文件
	internal.ReadServiceIdFile()
	utils.Wg.Wait()

	_go.AddGoPackageToProtoDir()
	utils.Wg.Wait()

	internal.GenerateAllInOneDescriptor()
	utils.Wg.Wait()
	internal.ReadAllProtoFileServices()
	utils.Wg.Wait()
	cpp.BuildProtocCpp()
	_go.BuildGrpcServiceProto()
	utils.Wg.Wait()

	cpp.GenNodeUtil()
	utils.Wg.Wait()

	cpp.GenerateAllEventHandlers()
	utils.Wg.Wait()
	// 所有文件的proto读完以后
	internal.InitServiceId()
	utils.Wg.Wait()

	internal.WriteServiceIdFile()
	utils.Wg.Wait()

	internal.WriteMethodFile()
	cpp.GeneratorHandler()
	utils.Wg.Wait()

	internal.GenerateServiceConstants()
	utils.Wg.Wait()
	// 所有service初始化完以后
	internal.WriteGoMessageId()
	utils.Wg.Wait()

	internal.WriteServiceRegisterInfoFile()
	database.GenerateDBResource()
	utils.Wg.Wait()

	_go.GoRobotHandlerGenerator()
	utils.Wg.Wait()
	_go.GoRobotTotalHandlerGenerator()
	utils.Wg.Wait()
	cpp.CppPlayerDataLoadGenerator()
	utils.Wg.Wait()
	cpp.CppGrpcCallClient()
	utils.Wg.Wait()

	// 打印总耗时
	log.Printf("Total execution time: %s\n", time.Since(start))

}
