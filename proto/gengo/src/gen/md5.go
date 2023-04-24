package gen

import (
	"bytes"
	"crypto/md5"
	"encoding/hex"
	"gengo/config"
	"io"
	"os"
)

func MD5Str(filePath string) (md5str string, err error) {
	file, err := os.Open(filePath)
	if err != nil {
		return "", err
	}
	hash := md5.New()
	_, err = io.Copy(hash, file)
	return hex.EncodeToString(hash.Sum(nil)), err
}

func Compare(dstFilePath string, srcFilePath string) (same bool, err error) {
	srcMd5, err := MD5Str(srcFilePath)
	if err != nil {
		return false, err
	}
	dstMd5, err := MD5Str(dstFilePath)
	if err != nil {
		return false, err
	}
	if srcMd5 != dstMd5 {
		return false, nil
	}
	return true, err
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

func Md5Copy(dstFilePath string, srcFilePath string) (copy bool, err error) {
	same, err := Compare(dstFilePath, srcFilePath)
	if err != nil {
		return false, err
	}
	if !same {
		return false, err
	}
	_, err = Copy(srcFilePath, dstFilePath)
	return true, err
}

func WriteToMd5ExFile(filePath string) (err error) {
	md5Str, err := MD5Str(filePath)
	if err != nil {
		return err
	}
	md5FilePath := filePath + config.Md5Ex
	err = os.WriteFile(md5FilePath, []byte(md5Str), 0666)
	return err
}

func CompareByMd5Ex(dstFilePath string, srcFilePath string) (same bool, err error) {
	md5SrcFilePath := srcFilePath + config.Md5Ex
	srcByteMd5, err := os.ReadFile(md5SrcFilePath)
	srcMd5 := bytes.NewBuffer(srcByteMd5).String()
	if err != nil {
		return false, err
	}
	dstMd5, err := MD5Str(dstFilePath)
	if err != nil {
		return false, err
	}
	if srcMd5 != dstMd5 {
		return false, nil
	}
	return true, err
}

func MD5CopyByMd5Ex(dstFilePath string, srcFilePath string) (copy bool, err error) {
	same, err := CompareByMd5Ex(dstFilePath, srcFilePath)
	if err != nil {
		return false, err
	}
	if !same {
		return false, err
	}
	_, err = Copy(srcFilePath, dstFilePath)
	return true, err
}
