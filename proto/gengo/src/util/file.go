package util

import (
	"io"
	"os"
	"path/filepath"
)

func FileExists(filename string) bool {
	info, err := os.Stat(filename)
	if os.IsNotExist(err) {
		return false
	}
	return !info.IsDir()
}

func IsProtoFile(fd os.DirEntry) bool {
	if fd.IsDir() || filepath.Ext(fd.Name()) != ".proto" {
		return false
	}
	return true
}

func Copy(dst string, src string) (written int64, err error) {
	fileDst, err := os.Open(dst)
	if err != nil {
		return 0, err
	}
	fileSrc, err := os.Open(src)
	if err != nil {
		return 0, err
	}
	written, err = io.Copy(fileDst, fileSrc)
	return written, err
}
