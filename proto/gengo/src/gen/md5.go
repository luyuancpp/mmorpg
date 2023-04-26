package gen

import (
	"bytes"
	"crypto/md5"
	"encoding/hex"
	"gengo/config"
	"gengo/util"
	"io"
	"os"
	"strings"
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

func Md5Copy(dstFilePath string, srcFilePath string) (copy bool, err error) {
	same, err := Compare(dstFilePath, srcFilePath)
	if same {
		return false, err
	}
	_, err = util.Copy(dstFilePath, srcFilePath)
	return true, err
}

func WriteToMd5ExFile(filePath string, md5FilePath string) (err error) {
	md5Str, err := MD5Str(filePath)
	if err != nil {
		return err
	}
	err = os.WriteFile(md5FilePath, []byte(md5Str), 0666)
	//fmt.Println("Generator md5 file for", filePath, "->", md5FilePath)
	return err
}

func CompareByMd5Ex(dstFilePath string, md5SrcFilePath string) (same bool, err error) {
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

func GetMd5FileName(dstFilePath string) (filename string) {
	return strings.Replace(dstFilePath, config.ProjectDir, config.Md5Dir, 1)
}

func GetMd5ExFileName(dstFilePath string) (filename string) {
	return GetMd5FileName(dstFilePath) + config.Md5Ex
}

func MD5CopyByMd5Ex(dstFilePath string, srcFilePath string) (copy bool, err error) {
	same, err := CompareByMd5Ex(dstFilePath, srcFilePath)
	if err != nil {
		return false, err
	}
	if !same {
		return false, err
	}
	_, err = util.Copy(srcFilePath, dstFilePath)
	return true, err
}
