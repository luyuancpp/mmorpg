package internal

import (
	"bytes"
	"fmt"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"pbgen/internal/config"
	"pbgen/utils"
	"strings"
)

// BuildProtocDescAllInOne 生成合并的Protobuf描述符文件
func BuildProtocDescAllInOne() {
	utils.Wg.Add(1)

	go func() {
		defer utils.Wg.Done()
		if err := generateAllInOneDesc(); err != nil {
			log.Printf("描述符生成: 失败: %v", err)
		}
	}()
}

// generateAllInOneDesc 生成并解析全量描述符文件
func generateAllInOneDesc() error {
	// 步骤1：收集目标proto文件（去重）
	protoFiles, err := collectUniqueProtoFiles()
	if err != nil {
		return fmt.Errorf("收集Proto文件失败: %w", err)
	}
	if len(protoFiles) == 0 {
		log.Println("描述符生成: 未找到任何Proto文件")
		return nil
	}
	log.Printf("描述符生成: 共收集到%d个唯一Proto文件", len(protoFiles))

	// 步骤2：构建protoc命令参数
	args, err := buildDescriptorArgs(protoFiles)
	if err != nil {
		return fmt.Errorf("构建命令参数失败: %w", err)
	}

	// 步骤3：执行protoc命令
	if err := executeDescriptorCommand(args); err != nil {
		return fmt.Errorf("执行protoc失败: %w", err)
	}

	// 步骤4：读取并解析描述符文件
	if err := parseDescriptorFile(); err != nil {
		return fmt.Errorf("解析描述符文件失败: %w", err)
	}

	log.Printf("描述符生成: 成功，输出路径=%s", config.AllInOneProtoDescFile)
	return nil
}

// collectUniqueProtoFiles 收集所有唯一的Proto文件
func collectUniqueProtoFiles() ([]string, error) {
	protoFileSet := make(map[string]struct{}) // 用于去重
	var allProtoFiles []string

	for _, dir := range config.ProtoDirs {
		fileEntries, err := os.ReadDir(dir)
		if err != nil {
			log.Printf("描述符生成: 读取目录[%s]失败，跳过: %v", dir, err)
			continue
		}

		for _, entry := range fileEntries {
			if utils.IsProtoFile(entry) {
				absPath, err := filepath.Abs(filepath.Join(dir, entry.Name()))
				if err != nil {
					log.Printf("描述符生成: 获取文件[%s]绝对路径失败，跳过: %v", entry.Name(), err)
					continue
				}
				absPath = filepath.ToSlash(absPath)

				if _, exists := protoFileSet[absPath]; !exists {
					protoFileSet[absPath] = struct{}{}
					allProtoFiles = append(allProtoFiles, absPath)
				}
			}
		}
	}

	return allProtoFiles, nil
}

// buildDescriptorArgs 构建生成描述符的protoc参数
func buildDescriptorArgs(protoFiles []string) ([]string, error) {
	descOutput := filepath.ToSlash(config.AllInOneProtoDescFile)

	// 基础选项
	args := []string{
		"--descriptor_set_out=" + descOutput,
		"--include_imports",     // 包含所有依赖的描述符
		"--include_source_info", // 包含源码信息，便于调试
	}

	// 添加导入路径
	importPaths := []string{
		config.ProtoParentIncludePathDir,
		config.ProtoBufferDirectory,
	}
	for _, ip := range importPaths {
		if ip == "" {
			continue // 跳过空路径
		}

		absIP, err := resolveAbsPath(ip, "描述符生成导入路径")
		if err != nil {
			log.Printf("描述符生成: 导入路径[%s]无效，跳过: %v", ip, err)
			continue
		}
		args = append(args, "--proto_path="+filepath.ToSlash(absIP))
	}

	// 添加目标proto文件
	args = append(args, protoFiles...)
	return args, nil
}

// executeDescriptorCommand 执行生成描述符的protoc命令
func executeDescriptorCommand(args []string) error {
	protocPath := "protoc" // 默认使用系统protoc
	cmd := exec.Command(protocPath, args...)

	var stdout, stderr bytes.Buffer
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr

	log.Printf("描述符生成: 执行命令: %s %s", cmd.Path, strings.Join(cmd.Args[1:], " "))
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("命令执行失败: 错误=%v,  stderr=%s", err, stderr.String())
	}

	if stdout.Len() > 0 {
		log.Printf("描述符生成: 命令输出: %s", stdout.String())
	}
	return nil
}

// parseDescriptorFile 解析生成的描述符文件
func parseDescriptorFile() error {
	// 读取文件内容
	data, err := os.ReadFile(config.AllInOneProtoDescFile)
	if err != nil {
		return fmt.Errorf("读取文件失败: %w", err)
	}
	log.Printf("描述符生成: 读取文件成功，大小=%d字节", len(data))

	// 解析为FileDescriptorSet
	if FdSet == nil {
		FdSet = &descriptorpb.FileDescriptorSet{}
	}
	if err := proto.Unmarshal(data, FdSet); err != nil {
		return fmt.Errorf("反序列化失败: %w，可能是文件损坏或版本不兼容", err)
	}

	// 验证解析结果
	log.Printf("描述符生成: 成功解析，包含%d个文件描述符", len(FdSet.GetFile()))
	for _, fileDesc := range FdSet.GetFile() {
		// 打印关键文件信息
		if strings.Contains(fileDesc.GetName(), "proto/service/go/grpc") ||
			strings.Contains(fileDesc.GetName(), "proto/common") {
			log.Printf("  描述符文件: %s，包含%d个消息类型",
				fileDesc.GetName(), len(fileDesc.GetMessageType()))
		}
	}

	return nil
}
