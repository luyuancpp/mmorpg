package main

import (
	"fmt"
	config "gengo/config"
	gen "gengo/gen"
	"io/ioutil"
	"os"
	"path"
)

func MakeProjectMd5Dir(src string, dst string) error {

	var err error
	var fds []os.FileInfo
	var srcinfo os.FileInfo

	if srcinfo, err = os.Stat(src); err != nil {
		return err
	}

	if err = os.MkdirAll(dst, srcinfo.Mode()); err != nil {
		return err
	}

	if fds, err = ioutil.ReadDir(src); err != nil {
		return err
	}
	for _, fd := range fds {
		srcfp := path.Join(src, fd.Name())
		dstfp := path.Join(dst, fd.Name())
		if !fd.IsDir() {
			continue
		}
		if err = MakeProjectMd5Dir(srcfp, dstfp); err != nil {
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
	md5str, _ := gen.FileMD5(config.ProjectDir + "autogen.sh")
	print(md5str)
}
