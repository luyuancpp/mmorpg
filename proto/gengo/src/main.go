package main

import (
	"fmt"
	"gengo/config"
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

func main() {
	os.MkdirAll(config.Md5Dir, os.FileMode(0777))

	for i := 0; i < len(config.ServerDirs); i++ {
		MakeProjectMd5Dir(config.ProjectDir+config.ServerDirs[i], config.Md5Dir+config.ServerDirs[i])
	}

	for i := 0; i < len(config.ProtoDirNames); i++ {
		os.MkdirAll(config.Md5Dir+config.ProtoDirNames[i], os.FileMode(0777))
	}
}
