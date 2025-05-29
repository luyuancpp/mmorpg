package main

import (
	"fmt"
	"log"
	"os"
	"path"
	"pbgen/config"
	"pbgen/internal"
	"pbgen/util"
)

func MakeProjectMd5Dir(src string, dst string) error {
	var err error
	var fds []os.DirEntry
	var srcFileInfo os.FileInfo

	if srcFileInfo, err = os.Stat(src); err != nil {
		return err
	}

	if err = os.MkdirAll(dst, srcFileInfo.Mode()); err != nil {
		return err
	}

	if fds, err = os.ReadDir(src); err != nil {
		return err
	}
	for _, fd := range fds {
		if !fd.IsDir() {
			continue
		}
		srcFp := path.Join(src, fd.Name())
		dstFp := path.Join(dst, fd.Name())
		if err = MakeProjectMd5Dir(srcFp, dstFp); err != nil {
			fmt.Println(err)
		}
	}
	return nil
}

func MakeProjectDir() {
	os.MkdirAll(config.PbcOutputDirectory, os.FileMode(0777))
	os.MkdirAll(config.PbcLuaDirectory, os.FileMode(0777))
	os.MkdirAll(config.ServiceInfoDirectory, os.FileMode(0777))
	os.MkdirAll(config.CppGenGrpcDirectory, os.FileMode(0777))

	for i := 0; i < len(config.ProtoDirectoryNames); i++ {
		config.ProtoDirs = append(config.ProtoDirs, config.ProtoDir+config.ProtoDirectoryNames[i])
	}
}

func main() {
	dir, err := os.Getwd()
	if err != nil {
		log.Fatal(err)
	}

	fmt.Println("Current working directory:", dir)

	MakeProjectDir()
	//开始读所有的proto文件
	internal.ReadServiceIdFile()
	internal.BuildProtocDesc()
	util.Wg.Wait()
	internal.ReadAllProtoFileServices()
	util.Wg.Wait()
	internal.BuildAllProtoc()
	util.Wg.Wait()

	internal.GenerateAllEventHandlers()
	//internal.WriteSol2LuaFile()
	util.Wg.Wait()
	//所有文件的proto读完以后
	internal.InitServiceId()
	internal.WriteServiceIdFile()
	internal.WriteMethodFile()
	internal.GenerateServiceConstants()
	util.Wg.Wait()
	//所有service初始化完以后
	internal.WriteGoMessageId()
	internal.WriteServiceRegisterInfoFile()
	//internal.WriteLuaServiceHeadHandlerFile()
	//internal.WriteClientServiceHeadHandlerFile()
	internal.GoRobotHandlerGenerator()
	util.Wg.Wait()
	internal.GoRobotTotalHandlerGenerator()
	internal.CppPlayerDataLoadGenerator()
	internal.CppGrpcCallClient()
	util.Wg.Wait()
}
