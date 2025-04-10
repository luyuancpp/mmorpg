package main

import (
	"fmt"
	"os"
	"path"
	"pbgen/config"
	"pbgen/gen"
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
	MakeProjectDir()
	//开始读所有的proto文件
	gen.ReadServiceIdFile()
	util.Wg.Wait()
	gen.BuildAllProtoc()
	util.Wg.Wait()
	gen.ReadAllProtoFileServices()
	util.Wg.Wait()

	//gen.WriteLoadClientLuaFile()
	gen.WriteEventHandlerFile()
	//gen.WriteSol2LuaFile()
	util.Wg.Wait()
	//所有文件的proto读完以后
	gen.InitServiceId()
	gen.WriteServiceIdFile()
	gen.WriteMethodFile()
	util.Wg.Wait()
	//所有service初始化完以后
	gen.WriteGoMessageId()
	gen.WriteServiceRegisterInfoFile()
	//gen.WriteLuaServiceHeadHandlerFile()
	//gen.WriteClientServiceHeadHandlerFile()
	gen.GoRobotHandlerGenerator()
	util.Wg.Wait()
	gen.GoRobotTotalHandlerGenerator()
	gen.CppPlayerDataLoadGenerator()
	gen.CppGrpcCallClient()
	util.Wg.Wait()
}
