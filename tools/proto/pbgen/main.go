package main

import (
	"fmt"
	"log"
	"net/http"
	_ "net/http/pprof"
	"os"
	"pbgen/internal"
	"pbgen/internal/config"
	"pbgen/util"
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
	// 开始读所有的proto文件
	internal.ReadServiceIdFile()
	util.Wg.Wait()

	internal.BuildProtocDescAllInOne()
	util.Wg.Wait()
	internal.ReadAllProtoFileServices()
	util.Wg.Wait()
	internal.BuildAllProtoc()
	util.Wg.Wait()

	internal.GenNodeUtil()
	util.Wg.Wait()

	internal.GenerateAllEventHandlers()
	util.Wg.Wait()
	// 所有文件的proto读完以后
	internal.InitServiceId()
	util.Wg.Wait()

	internal.WriteServiceIdFile()
	util.Wg.Wait()

	internal.WriteMethodFile()
	util.Wg.Wait()

	internal.GenerateServiceConstants()
	util.Wg.Wait()
	// 所有service初始化完以后
	internal.WriteGoMessageId()
	util.Wg.Wait()

	internal.WriteServiceRegisterInfoFile()
	util.Wg.Wait()

	internal.GoRobotHandlerGenerator()
	util.Wg.Wait()
	internal.GoRobotTotalHandlerGenerator()
	util.Wg.Wait()
	internal.CppPlayerDataLoadGenerator()
	util.Wg.Wait()
	internal.CppGrpcCallClient()
	util.Wg.Wait()

	// 打印总耗时
	log.Printf("Total execution time: %s\n", time.Since(start))

}
