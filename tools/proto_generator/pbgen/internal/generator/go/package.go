package _go

import (
	"bufio"
	"fmt"
	"os"
	"path/filepath"
	"strings"

	"go.uber.org/zap" // 引入zap用于结构化日志字段
	"pbgen/logger"    // 引入封装的logger包
)

// 生成正确格式的 option go_package（包含路径和包名）
// goPackagePath: 完整的Go包路径（如 "db/service/cpp/rpc/centre"）
// 返回格式: option go_package = "db/service/cpp/rpc/centre;centre";
func GenGoPackageOptWithPkg(goPackagePath string) string {
	// 从路径中提取最后一段作为包名（如从"db/service/cpp/rpc/centre"提取"centre"）
	pkgName := filepath.Base(goPackagePath)
	// 确保包名符合Go规范（替换可能的特殊字符）
	pkgName = strings.ReplaceAll(pkgName, "-", "_")
	pkgName = strings.ReplaceAll(pkgName, ".", "_")

	// 生成包含路径和包名的完整语句
	return fmt.Sprintf("option go_package = \"%s;%s\";", goPackagePath, pkgName)
}

func GenGoPackageOptWithAdjustedPath(goPackagePath string) string {
	// 分割路径为多个段
	parts := strings.Split(goPackagePath, "/")
	if len(parts) < 1 {
		// 空路径特殊处理
		return `option go_package = "unknown;unknown_proto";`
	}

	// 根据段数决定处理方式
	var remainingParts []string
	if len(parts) >= 2 {
		// 段数足够时，取parts[2:]
		remainingParts = parts[1:]
	} else if len(parts) == 1 {
		// 只有两段时，取空切片（避免parts[2:]越界）
		remainingParts = []string{}
	} else {
		// 只有一段时，直接在原路径处理
		pkgName := parts[0] + "_proto"
		pkgName = strings.ReplaceAll(pkgName, "-", "_")
		pkgName = strings.ReplaceAll(pkgName, ".", "_")
		return fmt.Sprintf("option go_package = \"%s\";", goPackagePath)
	}

	// 拼接新路径（remainingParts + 新的最后一段）
	fullParts := remainingParts
	newPath := strings.Join(fullParts, "/")

	return fmt.Sprintf("option go_package = \"%s\";", newPath)
}

// AddGoPackage 为指定proto文件添加option go_package
// protoFile: proto文件路径
// goPackagePath: 要添加的go_package路径（例如："example/proto/db"）
// 返回值: 是否实际添加了（true=添加，false=已存在）
func AddGoPackage(protoFile, goPackagePath string, isMulti bool) (bool, error) {
	// 读取文件内容
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

	// 检查是否已存在go_package
	for _, line := range lines {
		trimmed := strings.TrimSpace(line)
		if strings.HasPrefix(trimmed, "option go_package =") {
			// 已存在，直接返回
			return false, nil
		}
	}

	// 确定插入位置：syntax之后，package之前
	insertIndex := 0
	foundSyntax := false
	foundPackage := false

	for i, line := range lines {
		trimmed := strings.TrimSpace(line)

		// 标记syntax位置
		if strings.HasPrefix(trimmed, "syntax =") {
			foundSyntax = true
			insertIndex = i + 1 // 紧跟syntax之后
			continue
		}

		// 遇到package则插入到它前面
		if strings.HasPrefix(trimmed, "package ") {
			foundPackage = true
			insertIndex = i // 插入到package之前
			break
		}
	}

	// 如果没有找到syntax（不规范，但兼容处理）
	if !foundSyntax {
		// 如果有package，插在package前；否则插在文件开头
		if !foundPackage {
			insertIndex = 0
		}
	}

	// 构建要插入的行
	var goPackageLine string
	if isMulti {
		goPackageLine = GenGoPackageOptWithAdjustedPath(goPackagePath)
	} else {
		goPackageLine = GenGoPackageOptWithPkg(goPackagePath)
	}

	// 插入新行
	newLines := make([]string, 0, len(lines)+1)
	newLines = append(newLines, lines[:insertIndex]...)
	newLines = append(newLines, goPackageLine)
	newLines = append(newLines, lines[insertIndex:]...)

	// 写回文件
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
