package _go

import (
	"errors"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"pbgen/config"
	_config "pbgen/internal/config"
	"pbgen/internal/proto"
	utils2 "pbgen/internal/utils"
	"sync"
)

// GenerateGoProto 递归处理目录下Proto文件，生成Go GRPC代码
func GenerateGoProto(rootDir string) error {
	// 跳过Etcd服务相关目录
	if utils2.CheckEtcdServiceExistence(rootDir) {
		return nil
	}

	// 1. 收集非数据库Proto文件
	protoFiles, err := collectGoGrpcProtoFiles(rootDir)
	if err != nil {
		log.Fatal("Go GRPC生成: 收集Proto失败: %w", err)
	}
	if len(protoFiles) == 0 {
		log.Printf("Go GRPC生成: 目录[%s]无需要处理的Proto文件，跳过", rootDir)
		return nil
	}

	// 2. 解析输出目录
	nodeGoDir, err := utils2.ResolveAbsPath(_config.Global.Paths.NodeGoDir, "Go节点输出目录")
	if err != nil {
		return err
	}
	if err := utils2.EnsureDir(nodeGoDir); err != nil {
		log.Fatal("Go GRPC生成: 创建输出目录失败: %w", err)
	}

	// 3. 解析Proto根路径
	protoRootPath, err := utils2.ResolveAbsPath(filepath.Dir(rootDir), "Proto根目录")
	if err != nil {
		return err
	}

	// 4. 生成Go GRPC代码
	if err := GenerateGoGrpc(protoFiles, nodeGoDir, protoRootPath); err != nil {
		log.Fatal("Go GRPC生成: 代码生成失败: %w", err)
	}

	return nil
}

// GenerateRobotGoProto 递归处理目录下Proto文件，生成Go GRPC代码
func GenerateRobotGoProto(rootDir string) error {
	// 1. 收集非数据库Proto文件
	protoFiles, err := collectGoGrpcProtoFiles(rootDir)
	if err != nil {
		log.Fatal("Go GRPC生成: 收集Proto失败: %w", err)
	}
	if len(protoFiles) == 0 {
		log.Printf("Go GRPC生成: 目录[%s]无需要处理的Proto文件，跳过", rootDir)
		return nil
	}

	// 2. 解析输出目录
	nodeGoDir, err := utils2.ResolveAbsPath(_config.Global.Paths.ToolsDir, "Go节点输出目录")
	if err != nil {
		return err
	}
	if err := utils2.EnsureDir(nodeGoDir); err != nil {
		log.Fatal("Go GRPC生成: 创建输出目录失败: %w", err)
	}

	// 3. 解析Proto根路径
	protoRootPath, err := utils2.ResolveAbsPath(filepath.Dir(rootDir), "Proto根目录")
	if err != nil {
		return err
	}

	// 4. 生成Go GRPC代码
	if err := GenerateGoGrpc(protoFiles, nodeGoDir, protoRootPath); err != nil {
		log.Fatal("Go GRPC生成: 代码生成失败: %w", err)
	}

	return nil
}

// collectGoGrpcProtoFiles 递归收集目录下所有非数据库Proto文件
func collectGoGrpcProtoFiles(rootDir string) ([]string, error) {
	var protoFiles []string
	err := filepath.WalkDir(rootDir, func(path string, d os.DirEntry, err error) error {
		if err != nil {
			log.Fatal("访问路径[%s]失败: %w", path, err)
		}

		// 跳过目录、非Proto文件和数据库专用Proto文件
		if d.IsDir() || !utils2.IsProtoFile(d) || d.Name() == config.DbProtoFileName {
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

// GenerateGoGrpc 生成Go GRPC代码
func GenerateGoGrpc(protoFiles []string, outputDir string, protoRootPath string) error {
	// 提前校验空输入
	if len(protoFiles) == 0 {
		log.Fatal("Go GRPC生成: 没有需要处理的Proto文件")
	}

	// 1. 解析路径
	goOutputDir, err := utils2.ResolveAbsPath(outputDir, "Go GRPC输出目录")
	if err != nil {
		return err
	}

	protoRootDir, err := utils2.ResolveAbsPath(protoRootPath, "Proto根目录")
	if err != nil {
		return err
	}

	protoBufferDir, err := utils2.ResolveAbsPath(_config.Global.Paths.ProtobufDir, "ProtoBuffer目录")
	if err != nil {
		return err
	}

	// 2. 验证protoc路径
	protocPath, err := resolveProtocPath()
	if err != nil {
		log.Fatal("Go GRPC生成: 解析protoc路径失败: %w", err)
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
		absFile, err := utils2.ResolveAbsPath(file, "Go GRPC Proto源文件")
		if err != nil {
			log.Printf("Go GRPC生成: 跳过无效文件[%s]: %v", file, err)
			continue
		}
		args = append(args, absFile)
	}

	// 5. 执行命令
	return utils2.RunProtocWithPath(protocPath, args, "生成Go GRPC代码")
}

// resolveProtocPath 解析protoc可执行文件路径
func resolveProtocPath() (string, error) {
	return "protoc", nil
}

// AddGoPackageToProtoDir 为Proto文件添加go_package声明
func AddGoPackageToProtoDir() {
	utils2.Wg.Add(1)
	go func() {
		defer utils2.Wg.Done()
		grpcDirs := utils2.GetGRPCSubdirectoryNames()

		// 处理普通生成目录
		for _, dirName := range grpcDirs {
			destDir := proto.BuildGeneratorProtoPath(dirName)
			baseGoPackage := filepath.ToSlash(dirName)

			if err := addDynamicGoPackage(destDir, baseGoPackage, destDir, false); err != nil {
				log.Printf("GoPackage设置: 目录[%s]处理失败: %v", destDir, err)
			}
		}

		// 处理GoZero生成目录
		for _, dirName := range grpcDirs {
			destDir := proto.BuildGeneratorGoZeroProtoPath(dirName)
			baseGoPackage := filepath.ToSlash(dirName)

			if err := AddGoZeroPackageToProtos(destDir, baseGoPackage, destDir, true); err != nil {
				log.Printf("GoZero GoPackage设置: 目录[%s]处理失败: %v", destDir, err)
			}
		}

		destDir := proto.BuildGeneratorProtoPath(_config.Global.Paths.RobotDir)
		baseGoPackage := filepath.ToSlash(config.GoRobotPackage)

		if err := addDynamicGoPackage(destDir, baseGoPackage, destDir, false); err != nil {
			log.Printf("GoPackage设置: 目录[%s]处理失败: %v", destDir, err)
		}
	}()
}

// addDynamicGoPackage 为目录下所有文件生成基于相对路径的go_package
func addDynamicGoPackage(rootDir, baseGoPackage, currentDir string, isMulti bool) error {
	entries, err := os.ReadDir(currentDir)
	if err != nil {
		log.Fatal("读取目录[%s]失败: %w", currentDir, err)
	}

	for _, entry := range entries {
		fullPath := filepath.Join(currentDir, entry.Name())
		info, err := entry.Info()
		if err != nil {
			log.Fatal("获取文件信息[%s]失败: %w", fullPath, err)
		}

		if info.IsDir() {
			// 递归处理子目录
			if err := addDynamicGoPackage(rootDir, baseGoPackage, fullPath, isMulti); err != nil {
				return err
			}
		} else if filepath.Ext(fullPath) == ".proto" {
			// 处理Proto文件
			if err := processProtoFileForGoPackage(rootDir, baseGoPackage, fullPath, isMulti, false); err != nil {
				return err
			}
		}
	}
	return nil
}

// AddGoZeroPackageToProtos 为GoZero生成目录处理go_package
func AddGoZeroPackageToProtos(rootDir, baseGoPackage, currentDir string, isMulti bool) error {
	entries, err := os.ReadDir(currentDir)
	if err != nil {
		log.Fatal("读取目录[%s]失败: %w", currentDir, err)
	}

	for _, entry := range entries {
		fullPath := filepath.Join(currentDir, entry.Name())
		info, err := entry.Info()
		if err != nil {
			log.Fatal("获取文件信息[%s]失败: %w", fullPath, err)
		}

		if info.IsDir() {
			// 递归处理子目录
			if err := AddGoZeroPackageToProtos(rootDir, baseGoPackage, fullPath, isMulti); err != nil {
				return err
			}
		} else if filepath.Ext(fullPath) == ".proto" {
			// 处理Proto文件
			if err := processProtoFileForGoPackage(rootDir, baseGoPackage, fullPath, isMulti, true); err != nil {
				return err
			}
		}
	}
	return nil
}

// processProtoFileForGoPackage 为单个Proto文件设置go_package
func processProtoFileForGoPackage(rootDir, baseGoPackage, filePath string, isMulti, isGoZero bool) error {
	// 计算相对路径
	relativePath, err := filepath.Rel(rootDir, filepath.Dir(filePath))
	if err != nil {
		log.Fatal("计算相对路径失败: %w", err)
	}

	// 生成go_package路径
	var goPackagePath string
	if relativePath == "." {
		// 文件在根目录
		if isGoZero {
			goPackagePath = baseGoPackage
		} else {
			goPackagePath = filepath.Join(baseGoPackage, _config.Global.FileExtensions.ProtoDirName)
		}
	} else {
		// 拼接基础路径和相对目录
		if isGoZero {
			goPackagePath = filepath.Join(baseGoPackage, filepath.ToSlash(relativePath))
		} else {
			goPackagePath = filepath.Join(
				baseGoPackage,
				_config.Global.FileExtensions.ProtoDirName,
				filepath.ToSlash(relativePath),
			)
		}
	}
	goPackagePath = filepath.ToSlash(goPackagePath)

	// 添加go_package到文件
	added, err := AddGoPackage(filePath, goPackagePath, isMulti)
	if err != nil {
		log.Fatal("设置go_package失败: %w", err)
	}

	if added {
		log.Printf("GoPackage设置: 文件[%s]设置为[%s]", filePath, goPackagePath)
	} else {
		log.Printf("GoPackage设置: 文件[%s]已存在，跳过", filePath)
	}
	return nil
}

// processGrpcDir 处理单个GRPC目录
func processGrpcDir(dirName string) error {
	destDir := proto.BuildGeneratorProtoPath(dirName)
	if _, err := os.Stat(destDir); errors.Is(err, os.ErrNotExist) {
		log.Fatal("目录[%s]不存在", destDir)
	}

	return GenerateGoProto(destDir)
}

// BuildGrpcServiceProto 并发处理所有GRPC目录
func BuildGrpcServiceProto() {
	grpcDirs := utils2.GetGRPCSubdirectoryNames()
	var wg sync.WaitGroup

	for _, dirName := range grpcDirs {
		wg.Add(1)

		// 传递当前目录名副本到goroutine，避免循环变量捕获问题
		go func(currentDir string) {
			defer wg.Done()
			if err := processGrpcDir(currentDir); err != nil {
				log.Printf("GRPC服务构建: 目录[%s]处理失败: %v", currentDir, err)
			} else {
				log.Printf("GRPC服务构建: 目录[%s]处理完成", currentDir)
			}
		}(dirName)
	}

	wg.Add(1)

	// 传递当前目录名副本到goroutine，避免循环变量捕获问题
	go func(currentDir string) {
		defer wg.Done()
		if err := GenerateRobotGoProto(currentDir); err != nil {
			log.Printf("GRPC服务构建: 目录[%s]处理失败: %v", currentDir, err)
		} else {
			log.Printf("GRPC服务构建: 目录[%s]处理完成", currentDir)
		}
	}(proto.BuildGeneratorProtoPath(_config.Global.Paths.RobotDir))

	wg.Wait()
	log.Println("GRPC服务构建: 所有目录处理完成")
}

// generateGameGrpcGo 为游戏GRPC生成多节点Go代码
func generateGameGrpcGo(protoFiles []string) error {
	// 1. 获取所有GRPC节点目录
	grpcNodes := utils2.GetGRPCSubdirectoryNames()
	if len(grpcNodes) == 0 {
		log.Println("Go生成: 未找到GRPC节点目录，跳过")
		return nil
	}

	// 2. 为每个节点生成专属代码
	for _, nodeName := range grpcNodes {
		nodeOutputDir := filepath.Join(_config.Global.Paths.NodeGoDir, nodeName)
		nodeOutputDir, err := utils2.ResolveAbsPath(nodeOutputDir, "节点game_rpc代码目录")
		if err != nil {
			log.Fatal("解析节点game_rpc代码目录: %w", err)
		}
		if err := utils2.EnsureDir(nodeOutputDir); err != nil {
			log.Printf("Go生成: 创建节点[%s]目录失败: %v，跳过", nodeName, err)
			continue
		}

		// 生成节点Go GRPC代码
		if err := GenerateGoGrpc(protoFiles, nodeOutputDir, _config.Global.Paths.GameRpcProtoPath); err != nil {
			log.Printf("Go生成: 节点[%s]代码生成失败: %v，跳过", nodeName, err)
			continue
		}
		log.Printf("Go生成: 节点[%s]代码生成成功", nodeName)
	}

	// 3. 确保机器人代码目录存在
	robotDir, err := utils2.ResolveAbsPath(config.RobotGoOutputGeneratedDirectory, "机器人代码目录")
	if err != nil {
		log.Fatal("解析机器人目录失败: %w", err)
	}
	if err := utils2.EnsureDir(robotDir); err != nil {
		log.Fatal("创建机器人目录失败: %w", err)
	}

	if err := GenerateGoGrpc(protoFiles, robotDir, _config.Global.Paths.GameRpcProtoPath); err != nil {
		log.Printf("Go生成: 节点[%s]代码生成失败: %v，跳过", robotDir, err)
		return err
	}

	log.Println("Go生成: 所有游戏GRPC节点代码生成完成")
	return nil
}

// generateGameGrpcImpl 游戏GRPC生成核心逻辑
func generateGameGrpcImpl() error {
	// 1. 解析游戏Proto文件路径
	gameProtoPath, err := proto.ResolveGameProtoPath()
	if err != nil {
		log.Fatal("解析Proto路径失败: %w", err)
	}
	protoFiles := []string{gameProtoPath}

	// 2. 生成Go节点代码
	if err := generateGameGrpcGo(protoFiles); err != nil {
		log.Fatal("Go代码生成失败: %w", err)
	}

	return nil
}

// GenerateGameGrpc 生成游戏GRPC代码（C++序列化+Go节点代码）
func GenerateGameGrpc(wg *sync.WaitGroup) {
	if wg == nil {
		wg = &sync.WaitGroup{}
	}
	wg.Add(1)

	go func() {
		defer wg.Done()
		if err := generateGameGrpcImpl(); err != nil {
			log.Printf("游戏GRPC生成: 整体失败: %v", err)
		}
	}()
}
