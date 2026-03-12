package _go

import (
	"bufio"
	"fmt"
	"os"
	"path/filepath"
	"strings"

	"go.uber.org/zap"
	"pbgen/logger"
)

// GenGoPackageOptWithPkg generates a go_package option with path and package name.
func GenGoPackageOptWithPkg(goPackagePath string) string {
	pkgName := filepath.Base(goPackagePath)
	pkgName = strings.ReplaceAll(pkgName, "-", "_")
	pkgName = strings.ReplaceAll(pkgName, ".", "_")

	return fmt.Sprintf("option go_package = \"%s;%s\";", goPackagePath, pkgName)
}

func GenGoPackageOptWithAdjustedPath(goPackagePath string) string {
	parts := strings.Split(goPackagePath, "/")
	if len(parts) < 1 {
		return `option go_package = "unknown;unknown_proto";`
	}

	var remainingParts []string
	if len(parts) >= 2 {
		remainingParts = parts[1:]
	} else if len(parts) == 1 {
		remainingParts = []string{}
	} else {
		pkgName := parts[0] + "_proto"
		pkgName = strings.ReplaceAll(pkgName, "-", "_")
		pkgName = strings.ReplaceAll(pkgName, ".", "_")
		return fmt.Sprintf("option go_package = \"%s\";", goPackagePath)
	}

	fullParts := remainingParts
	newPath := strings.Join(fullParts, "/")

	return fmt.Sprintf("option go_package = \"%s\";", newPath)
}

// AddGoPackage inserts an option go_package line into a proto file if not present.
func AddGoPackage(protoFile, goPackagePath string, isMulti bool) (bool, error) {
	file, err := os.Open(protoFile)
	if err != nil {
		logger.Global.Fatal("打开Proto文件失败",
			zap.String("文件路径", protoFile),
			zap.Error(err),
		)
	}
	defer file.Close()

	var lines []string
	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		lines = append(lines, scanner.Text())
	}
	if err := scanner.Err(); err != nil {
		logger.Global.Fatal("读取Proto文件内容失败",
			zap.String("文件路径", protoFile),
			zap.Error(err),
		)
	}

	for _, line := range lines {
		trimmed := strings.TrimSpace(line)
		if strings.HasPrefix(trimmed, "option go_package =") {
			return false, nil
		}
	}

	insertIndex := 0
	foundSyntax := false
	foundPackage := false

	for i, line := range lines {
		trimmed := strings.TrimSpace(line)

		if strings.HasPrefix(trimmed, "syntax =") {
			foundSyntax = true
			insertIndex = i + 1 // 紧跟syntax之后
			continue
		}

		if strings.HasPrefix(trimmed, "package ") {
			foundPackage = true
			insertIndex = i // 插入到package之前
			break
		}
	}

	if !foundSyntax {
		if !foundPackage {
			insertIndex = 0
		}
	}

	var goPackageLine string
	if isMulti {
		goPackageLine = GenGoPackageOptWithAdjustedPath(goPackagePath)
	} else {
		goPackageLine = GenGoPackageOptWithPkg(goPackagePath)
	}

	newLines := make([]string, 0, len(lines)+1)
	newLines = append(newLines, lines[:insertIndex]...)
	newLines = append(newLines, goPackageLine)
	newLines = append(newLines, lines[insertIndex:]...)

	output, err := os.Create(protoFile)
	if err != nil {
		logger.Global.Fatal("创建Proto文件写入句柄失败",
			zap.String("文件路径", protoFile),
			zap.Error(err),
		)
	}
	defer output.Close()

	writer := bufio.NewWriter(output)
	for _, line := range newLines {
		if _, err := writer.WriteString(line + "\n"); err != nil {
			logger.Global.Fatal("写入Proto文件内容失败",
				zap.String("文件路径", protoFile),
				zap.Error(err),
			)
		}
	}
	if err := writer.Flush(); err != nil {
		logger.Global.Fatal("刷新Proto文件写入缓存失败",
			zap.String("文件路径", protoFile),
			zap.Error(err),
		)
	}

	return true, nil
}
