package util

import (
	"bufio"
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

func Copy(dst string, src string) (written int64, err error) {
	sourceFileStat, err := os.Stat(src)
	if err != nil {
		return 0, fmt.Errorf("failed to get file info for %s: %v", src, err)
	}
	if !sourceFileStat.Mode().IsRegular() {
		return 0, fmt.Errorf("%s is not a regular file", src)
	}
	source, err := os.Open(src)
	if err != nil {
		return 0, fmt.Errorf("failed to open source file %s: %v", src, err)
	}
	defer source.Close()

	destination, err := os.Create(dst)
	if err != nil {
		return 0, fmt.Errorf("failed to create destination file %s: %v", dst, err)
	}
	defer destination.Close()

	nBytes, err := io.Copy(destination, source)
	if err != nil {
		return nBytes, fmt.Errorf("failed to copy data from %s to %s: %v", src, dst, err)
	}
	log.Printf("Copied %s -> %s\n", src, dst)
	return nBytes, nil
}

func ReadCodeSectionsFromFile(cppFileName string, codeCount int) ([]string, error) {
	var yourCodes []string
	fd, err := os.Open(cppFileName)
	if err != nil {
		for i := 0; i < codeCount; i++ {
			yourCodes = append(yourCodes, config.YourCodePair)
		}
		return yourCodes, fmt.Errorf("failed to open file %s: %v", cppFileName, err)
	}
	defer fd.Close()

	scanner := bufio.NewScanner(fd)
	var currentSection strings.Builder
	for scanner.Scan() {
		line := scanner.Text() + "\n"
		if strings.Contains(line, config.YourCodeBegin) {
			currentSection.WriteString(line)
		} else if strings.Contains(line, config.YourCodeEnd) {
			currentSection.WriteString(line)
			yourCodes = append(yourCodes, currentSection.String())
			currentSection.Reset()
		} else if len(yourCodes) > 0 {
			currentSection.WriteString(line)
		}
	}

	if len(yourCodes) < codeCount {
		addCount := codeCount - len(yourCodes)
		for i := 0; i < addCount; i++ {
			yourCodes = append(yourCodes, config.YourCodePair)
		}
	}

	return yourCodes, nil
}
