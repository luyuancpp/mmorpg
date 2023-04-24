package util

import (
	"io"
	"os"
)

func FileExists(filename string) bool {
	info, err := os.Stat(filename)
	if os.IsNotExist(err) {
		return false
	}
	return !info.IsDir()
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
