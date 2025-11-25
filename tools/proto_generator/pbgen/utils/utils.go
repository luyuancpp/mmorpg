package utils

import (
	"bytes"
	"fmt"
	"log"
	"os"
	"os/exec"
	"path/filepath"
)

// EnsureDir 确保目录存在，不存在则创建（权限默认0755）
func EnsureDir(dirPath string) error {
	if err := os.MkdirAll(dirPath, 0755); err != nil {
		return fmt.Errorf("创建目录[%s]失败: %w", dirPath, err)
	}
	return nil
}

// CollectProtoFiles 收集指定目录下所有.proto文件（返回绝对路径）
func CollectProtoFiles(dirPath string) ([]string, error) {
	fileEntries, err := os.ReadDir(dirPath)
	if err != nil {
		return nil, fmt.Errorf("读取目录[%s]失败: %w", dirPath, err)
	}

	var protoFiles []string
	for _, entry := range fileEntries {
		if IsProtoFile(entry) {
			absPath, err := filepath.Abs(filepath.Join(dirPath, entry.Name()))
			if err != nil {
				log.Printf("Proto收集: 获取文件[%s]绝对路径失败: %v，跳过", entry.Name(), err)
				continue
			}
			protoFiles = append(protoFiles, absPath)
		}
	}
	return protoFiles, nil
}

// ResolveAbsPath 将路径转换为绝对路径，并返回带描述的错误
func ResolveAbsPath(path string, desc string) (string, error) {
	absPath, err := filepath.Abs(path)
	if err != nil {
		return "", fmt.Errorf("解析[%s]路径失败: 路径=%s, 错误=%w", desc, path, err)
	}
	return absPath, nil
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

	log.Printf("protoc执行: %s, 命令=%s", actionDesc, cmd.String())
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("protoc执行失败: 动作=%s, 错误=%v, 错误输出=%s", actionDesc, err, stderr.String())
	}

	if stdout.Len() > 0 {
		log.Printf("protoc成功: %s, 输出=%s", actionDesc, stdout.String())
	}
	return nil
}

// CopyProtoDir 拷贝Proto目录到目标目录（保持目录结构）
func CopyProtoDir(srcDir, destDir string) error {
	if err := EnsureDir(destDir); err != nil {
		return err
	}
	if err := CopyLocalDir(srcDir, destDir); err != nil {
		return fmt.Errorf("拷贝Proto目录: 源=%s -> 目标=%s, 错误=%w", srcDir, destDir, err)
	}
	log.Printf("拷贝Proto目录成功: 源=%s -> 目标=%s", srcDir, destDir)
	return nil
}
