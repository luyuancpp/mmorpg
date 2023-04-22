package gen

import (
	"crypto/md5"
	"encoding/hex"
	"io"
	"os"
)

func FileMD5(filePath string) (md5str string, err error) {
	file, err := os.Open(filePath)
	if err != nil {
		return "", err
	}
	hash := md5.New()
	_, err = io.Copy(hash, file)
	return hex.EncodeToString(hash.Sum(nil)), err
}

// return true if md5 is same otherwise return false
func Md5Compare(dstFilePath string, srcFilePath string) (same bool, err error) {
	srcMd5, err := FileMD5(srcFilePath)
	if err != nil {
		return false, err
	}
	dstMd5, err := FileMD5(dstFilePath)
	if err != nil {
		return false, err
	}
	if srcMd5 != dstMd5 {
		return false, nil
	}
	return true, nil
}

func Md5Copy(dstFilePath string, srcFilePath string) (copy bool, err error) {
	same, err := Md5Compare(dstFilePath, srcFilePath)
	if err != nil {
		return false, err
	}
	if !same {
		return false, err
	}
	_, err = GenCopy(srcFilePath, dstFilePath)
	return true, err
}

func GenCopy(dst string, src string) (written int64, err error) {
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
