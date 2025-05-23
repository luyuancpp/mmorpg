package util

import (
	"crypto/md5"
	"encoding/hex"
	"io"
	"log"
	"os"
	"pbgen/config"
	"strings"
)

func GetMd5(data string) (md5str string) {
	hash := md5.New()
	hash.Write([]byte(data))
	return hex.EncodeToString(hash.Sum(nil))
}

func GetFileMd5(filePath string) (md5str string, err error) {
	file, err := os.Open(filePath)
	if err != nil {
		return "", err
	}
	hash := md5.New()
	_, _ = io.Copy(hash, file)
	return hex.EncodeToString(hash.Sum(nil)), nil
}

func CompareFileByMd5(dstFilePath string, srcFilePath string) (same bool, err error) {
	srcMd5, err := GetFileMd5(srcFilePath)
	if err != nil {
		return false, err
	}
	dstMd5, err := GetFileMd5(dstFilePath)
	if err != nil {
		return false, err
	}
	if srcMd5 != dstMd5 {
		return false, nil
	}
	return true, err
}

func CompareFileWithMd5String(dstFilePath string, srcMd5 string) (same bool, err error) {
	dstMd5, err := GetFileMd5(dstFilePath)
	if err != nil {
		return false, err
	}
	if srcMd5 != dstMd5 {
		return false, nil
	}
	return true, err
}

func CopyFileByMd5(dstFilePath string, srcFilePath string) (copy bool, err error) {
	same, err := CompareFileByMd5(dstFilePath, srcFilePath)
	if same {
		return false, err
	}
	_, err = Copy(dstFilePath, srcFilePath)
	return true, err
}

func WriteToMd5ExFile(filePath string, md5FilePath string) (err error) {
	md5Str, err := GetFileMd5(filePath)
	if err != nil {
		return err
	}
	err = os.WriteFile(md5FilePath, []byte(md5Str), 0666)
	//fmt.Println("Generator md5 file for", filePath, "->", md5FilePath)
	return err
}

func AreFileSizesEqual(file1, file2 string) bool {
	info1, err1 := os.Stat(file1)
	info2, err2 := os.Stat(file2)

	if err1 != nil || err2 != nil {
		return false
	}

	return info1.Size() == info2.Size()
}

func IsSameMD5(dstFilePath string, srcFilePath string) (same bool, err error) {
	if !AreFileSizesEqual(dstFilePath, srcFilePath) {
		return false, nil
	}

	srcMd5, err := GetFileMd5(srcFilePath)
	if err != nil {
		return false, err
	}
	dstMd5, err := GetFileMd5(dstFilePath)
	if err != nil {
		return false, err
	}

	if srcMd5 != dstMd5 {
		return false, nil
	}
	return true, err
}

func GetMd5FileName(dstFilePath string) (filename string) {
	return strings.Replace(dstFilePath, config.ProjectDir, config.TempFileGenerationDir, 1)
}

func Md5CopyByMd5Ex(dstFilePath string, srcFilePath string) (copy bool, err error) {
	same, err := IsSameMD5(dstFilePath, srcFilePath)
	if err != nil {
		return false, err
	}
	if !same {
		return false, err
	}
	_, err = Copy(dstFilePath, srcFilePath)
	return true, err
}

func WriteMd5Data2File(dstFilePath string, data string) {
	if same, _ := CompareFileWithMd5String(dstFilePath, GetMd5(data)); same {
		return
	}
	os.WriteFile(GetMd5FileName(dstFilePath), []byte(data), 0666)
	os.WriteFile(dstFilePath, []byte(data), 0666)
	log.Default().Println("Copied", GetMd5FileName(dstFilePath), " -> ", dstFilePath)
	return
}
