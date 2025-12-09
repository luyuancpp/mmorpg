package utils

import (
	"bytes"
	"os"
	"os/exec"
	"path/filepath"

	"go.uber.org/zap" // 引入zap用于结构化日志字段
	"pbgen/logger"    // 引入封装的logger包
)

// EnsureDir 确保目录存在，不存在则创建（权限默认0755）
func EnsureDir(dirPath string) error {
	if err := os.MkdirAll(dirPath, 0755); err != nil {
		logger.Global.Fatal("创建目录失败",
			zap.String("目录路径", dirPath),
			zap.Error(err),
		)
	}
	return nil
}

// CollectProtoFiles 收集指定目录下所有.proto文件（返回绝对路径）
func CollectProtoFiles(dirPath string) ([]string, error) {
	fileEntries, err := os.ReadDir(dirPath)
	if err != nil {
		logger.Global.Fatal("读取目录失败",
			zap.String("目录路径", dirPath),
			zap.Error(err),
		)
	}

	var protoFiles []string
	for _, entry := range fileEntries {
		if IsProtoFile(entry) {
			absPath, err := filepath.Abs(filepath.Join(dirPath, entry.Name()))
			if err != nil {
				logger.Global.Warn("Proto收集: 获取绝对路径失败，跳过",
					zap.String("文件名", entry.Name()),
					zap.String("目录", dirPath),
					zap.Error(err),
				)
				continue
			}
			protoFiles = append(protoFiles, absPath)
		}
	}
	return protoFiles, nil
}

// RunProtoc 执行protoc命令（默认使用系统PATH中的protoc）
func RunProtoc(args []string, actionDesc string) error {
	cmd := exec.Command("protoc", args...)
	return ExecuteProtocCmd(cmd, actionDesc)
}

// RunProtocWithPath 执行protoc命令（指定protoc路径）
func RunProtocWithPath(protocPath string, args []string, actionDesc string) error {
	cmd := exec.Command(protocPath, args...)
	return ExecuteProtocCmd(cmd, actionDesc)
}

// ExecuteProtocCmd 执行protoc命令并处理输出和错误
func ExecuteProtocCmd(cmd *exec.Cmd, actionDesc string) error {
	var stdout, stderr bytes.Buffer
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr

	logger.Global.Info("protoc执行",
		zap.String("动作描述", actionDesc),
		zap.String("命令", cmd.String()),
	)

	if err := cmd.Run(); err != nil {
		logger.Global.Fatal("protoc执行失败",
			zap.String("动作描述", actionDesc),
			zap.Error(err),
			zap.String("错误输出", stderr.String()),
			zap.String("命令", cmd.String()),
		)
	}

	if stdout.Len() > 0 {
		logger.Global.Info("protoc执行成功",
			zap.String("动作描述", actionDesc),
			zap.String("标准输出", stdout.String()),
		)
	}
	return nil
}
