package main

import (
	"fmt"
	"io/ioutil"
	"os"
	"path"
)

var projectDir = "../"
var md5Dir = string("md5/")
var serverDirs = [...]string{"common",
	"controller_server",
	"game_server",
	"gate_server",
	"login_server",
	"lobby_server",
	"database_server",
	"deploy_server",
	"client"}

// return true if md5 is same otherwise return false
func Md5Compare(dstFilePath string, srcFilePath string) (same bool, err error) {
	srcmd5, err := genmd5.FileMD5(srcFilePath)
	if err != nil {
		return false, err
	}
	dstmd5, err := genmd5.FileMD5(dstFilePath)
	if err != nil {
		return false, err
	}
	if srcmd5 != dstmd5 {
		return false, nil
	}
	return true, nil
}

func MakeProjectMd5Dir(src string, dst string) error {
	os.MkdirAll(md5Dir, os.FileMode(0777))

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
	for i := 0; i < len(serverDirs); i++ {
		MakeProjectMd5Dir(projectDir+serverDirs[i], md5Dir+serverDirs[i])
	}
	md5str, _ := genmd5.FileMD5(projectDir + "autogen.sh")
	print(md5str)
}
