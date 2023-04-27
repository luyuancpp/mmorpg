package main

import (
	"fmt"
	"gengo/config"
	"gengo/gen"
	"gengo/util"
	"os"
	"path"
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
	os.MkdirAll(config.PbcOutDir, os.FileMode(0777))
}

func MakeMd5Dir() {
	os.MkdirAll(config.Md5Dir, os.FileMode(0777))

	for i := 0; i < len(config.SourceNames); i++ {
		config.ProjectSourceDirs = append(config.ProjectSourceDirs, config.ProjectDir+config.SourceNames[i])
		config.ProjectSourceMd5Dirs = append(config.ProjectSourceMd5Dirs, config.Md5Dir+config.SourceNames[i])
	}

	for i := 0; i < len(config.ProtoDirNames); i++ {
		config.ProtoDirs = append(config.ProtoDirs, config.ProtoDir+config.ProtoDirNames[i])
		config.ProtoMd5Dirs = append(config.ProtoMd5Dirs, config.Md5Dir+config.ProtoDirNames[i])
	}

	for i := 0; i < len(config.SourceNames); i++ {
		MakeProjectMd5Dir(config.ProjectSourceDirs[i], config.ProjectSourceMd5Dirs[i])
	}

	for i := 0; i < len(config.ProtoMd5Dirs); i++ {
		os.MkdirAll(config.ProtoMd5Dirs[i], os.FileMode(0777))
	}
}

func main() {
	MakeProjectDir()
	MakeMd5Dir()
	//开始读所有的proto文件
	gen.ReadServiceIdFile()
	gen.ReadAllProtoFileServices()
	gen.BuildAllProtoc()
	util.Wg.Wait()
	//所有文件的proto读完以后
	gen.InitServiceId()
	gen.WriteServiceIdFile()
	gen.WriteServiceImplFile()
	gen.WriteMethodFile()
	util.Wg.Wait()
}
