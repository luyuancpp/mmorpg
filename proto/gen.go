package main

import (
	"crypto/md5"
	"encoding/hex"
	"fmt"
	"io"
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

func FileMD5(filePath string) (string, error) {
	file, err := os.Open(filePath)
	if err != nil {
		return "", err
	}
	hash := md5.New()
	_, _ = io.Copy(hash, file)
	return hex.EncodeToString(hash.Sum(nil)), nil
}

func GenCopy(dst string, src string) (written int64, err error) {
	filedst, dsterr := os.Open(dst)
	if dsterr != nil {
		return 0, dsterr
	}
	filesrc, srcerr := os.Open(src)
	if srcerr != nil {
		return 0, srcerr
	}
	written, err = io.Copy(filedst, filesrc)
	return written, err
}

func Md5Copy(dstFilePath string, srcFilePath string) error {
	srcmd5, srcerr := FileMD5(srcFilePath)
	if srcerr != nil {
		return srcerr
	}
	dstmd5, dsterr := FileMD5(dstFilePath)
	if dsterr != nil {
		return dsterr
	}
	if srcmd5 == dstmd5 {
		return nil
	}
	_, copyerr := GenCopy(srcFilePath, dstFilePath)
	return copyerr
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
	md5str, _ := FileMD5(projectDir + "autogen.sh")
	print(md5str)
}
