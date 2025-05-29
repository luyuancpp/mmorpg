package util

import (
	"fmt"
	"io"
	"log"
	"os"
	"path/filepath"
	"pbgen/config"
	"strings"
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

func IsLuaFile(fd os.DirEntry) bool {
	if fd.IsDir() || filepath.Ext(fd.Name()) != ".lua" {
		return false
	}
	return true
}

func IncludeName(path string, protoName string) string {
	pbcHeadName := strings.Replace(protoName, config.ProtoEx, config.ProtoPbhEx, 1)
	return config.IncludeBegin + strings.Replace(path, config.ProtoDir, "", 1) + pbcHeadName + "\"\n"
}

func Copy(dstFile string, srcFile string) (written int64, err error) {
	sourceFileStat, err := os.Stat(srcFile)
	if err != nil {
		return 0, fmt.Errorf("failed to get file info for %s: %v", srcFile, err)
	}

	if !sourceFileStat.Mode().IsRegular() {
		return 0, fmt.Errorf("%s is not a regular file", srcFile)
	}

	source, err := os.Open(srcFile)
	if err != nil {
		return 0, fmt.Errorf("failed to open source file %s: %v", srcFile, err)
	}
	defer source.Close()

	// ✅ 创建目标目录（如果不存在）
	dstDir := filepath.Dir(dstFile)
	if err := os.MkdirAll(dstDir, os.ModePerm); err != nil {
		return 0, fmt.Errorf("failed to create destination directory %s: %v", dstDir, err)
	}

	destination, err := os.Create(dstFile)
	if err != nil {
		return 0, fmt.Errorf("failed to create destination file %s: %v", dstFile, err)
	}
	defer destination.Close()

	nBytes, err := io.Copy(destination, source)
	if err != nil {
		return nBytes, fmt.Errorf("failed to copy data from %s to %s: %v", srcFile, dstFile, err)
	}

	log.Default().Println("Copied\n", srcFile, "->", dstFile)
	return nBytes, nil
}
