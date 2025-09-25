package internal

import (
	"fmt"
	"log"
	"os"
	"path/filepath"
	"pbgen/internal/config"
	"pbgen/utils"
)

// GenerateGoProto 递归处理目录下Proto文件，生成Go GRPC代码
func GenerateGoProto(rootDir string) error {
	// 跳过Etcd服务相关目录
	if utils.CheckEtcdServiceExistence(rootDir) {
		return nil
	}

	// 1. 收集非数据库Proto文件
	protoFiles, err := collectGoGrpcProtoFiles(rootDir)
	if err != nil {
		return fmt.Errorf("Go GRPC生成: 收集Proto失败: %w", err)
	}
	if len(protoFiles) == 0 {
		log.Printf("Go GRPC生成: 目录[%s]无需要处理的Proto文件，跳过", rootDir)
		return nil
	}

	// 2. 解析输出目录
	nodeGoDir, err := resolveAbsPath(config.NodeGoDirectory, "Go节点输出目录")
	if err != nil {
		return err
	}
	if err := ensureDir(nodeGoDir); err != nil {
		return fmt.Errorf("Go GRPC生成: 创建输出目录失败: %w", err)
	}

	// 3. 解析Proto根路径
	protoRootPath, err := resolveAbsPath(filepath.Dir(rootDir), "Proto根目录")
	if err != nil {
		return err
	}

	// 4. 生成Go GRPC代码
	if err := generateGoGrpc(protoFiles, nodeGoDir, protoRootPath); err != nil {
		return fmt.Errorf("Go GRPC生成: 代码生成失败: %w", err)
	}

	return nil
}

// GenerateRobotGoProto 递归处理目录下Proto文件，生成Go GRPC代码
func GenerateRobotGoProto(rootDir string) error {
	// 1. 收集非数据库Proto文件
	protoFiles, err := collectGoGrpcProtoFiles(rootDir)
	if err != nil {
		return fmt.Errorf("Go GRPC生成: 收集Proto失败: %w", err)
	}
	if len(protoFiles) == 0 {
		log.Printf("Go GRPC生成: 目录[%s]无需要处理的Proto文件，跳过", rootDir)
		return nil
	}

	// 2. 解析输出目录
	nodeGoDir, err := resolveAbsPath(config.ToolsDir, "Go节点输出目录")
	if err != nil {
		return err
	}
	if err := ensureDir(nodeGoDir); err != nil {
		return fmt.Errorf("Go GRPC生成: 创建输出目录失败: %w", err)
	}

	// 3. 解析Proto根路径
	protoRootPath, err := resolveAbsPath(filepath.Dir(rootDir), "Proto根目录")
	if err != nil {
		return err
	}

	// 4. 生成Go GRPC代码
	if err := generateGoGrpc(protoFiles, nodeGoDir, protoRootPath); err != nil {
		return fmt.Errorf("Go GRPC生成: 代码生成失败: %w", err)
	}

	return nil
}

// collectGoGrpcProtoFiles 递归收集目录下所有非数据库Proto文件
func collectGoGrpcProtoFiles(rootDir string) ([]string, error) {
	var protoFiles []string
	err := filepath.WalkDir(rootDir, func(path string, d os.DirEntry, err error) error {
		if err != nil {
			return fmt.Errorf("访问路径[%s]失败: %w", path, err)
		}

		// 跳过目录、非Proto文件和数据库专用Proto文件
		if d.IsDir() || !utils.IsProtoFile(d) || d.Name() == config.DbProtoFileName {
			return nil
		}

		// 收集符合条件的Proto文件
		absPath, err := filepath.Abs(path)
		if err != nil {
			log.Printf("Go GRPC收集: 获取文件[%s]绝对路径失败: %v，跳过", path, err)
			return nil
		}
		protoFiles = append(protoFiles, filepath.ToSlash(absPath))
		return nil
	})

	return protoFiles, err
}

// generateGoGrpc 生成Go GRPC代码
func generateGoGrpc(protoFiles []string, outputDir string, protoRootPath string) error {
	// 提前校验空输入
	if len(protoFiles) == 0 {
		return fmt.Errorf("Go GRPC生成: 没有需要处理的Proto文件")
	}

	// 1. 解析路径
	goOutputDir, err := resolveAbsPath(outputDir, "Go GRPC输出目录")
	if err != nil {
		return err
	}

	protoRootDir, err := resolveAbsPath(protoRootPath, "Proto根目录")
	if err != nil {
		return err
	}

	protoBufferDir, err := resolveAbsPath(config.ProtoBufferDirectory, "ProtoBuffer目录")
	if err != nil {
		return err
	}

	// 2. 验证protoc路径
	protocPath, err := resolveProtocPath()
	if err != nil {
		return fmt.Errorf("Go GRPC生成: 解析protoc路径失败: %w", err)
	}

	// 3. 构建protoc参数
	args := []string{
		fmt.Sprintf("--go_out=%s", goOutputDir),
		fmt.Sprintf("--go-grpc_out=%s", goOutputDir),
		fmt.Sprintf("--proto_path=%s", protoRootDir),
		fmt.Sprintf("--proto_path=%s", protoBufferDir),
	}

	// 4. 补充Proto文件路径
	for _, file := range protoFiles {
		absFile, err := resolveAbsPath(file, "Go GRPC Proto源文件")
		if err != nil {
			log.Printf("Go GRPC生成: 跳过无效文件[%s]: %v", file, err)
			continue
		}
		args = append(args, absFile)
	}

	// 5. 执行命令
	return runProtocWithPath(protocPath, args, "生成Go GRPC代码")
}

// resolveProtocPath 解析protoc可执行文件路径
func resolveProtocPath() (string, error) {
	return "protoc", nil
}
