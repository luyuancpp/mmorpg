package _go

import (
	"os"
	"path/filepath"
	"sync"

	"go.uber.org/zap" // 引入zap结构化日志字段
	_config "pbgen/internal/config"
	utils2 "pbgen/internal/utils"
	"pbgen/logger" // 引入全局logger包
)

func GenerateGoProto(rootDir string) error {
	// 跳过Etcd服务相关目录
	if utils2.HasEtcdService(rootDir) {
		logger.Global.Debug("跳过Etcd服务相关目录",
			zap.String("root_dir", rootDir),
		)
		return nil
	}

	// 1. 收集非数据库Proto文件
	protoFiles, err := collectGoGrpcProtoFiles(rootDir)
	if err != nil {
		logger.Global.Fatal("Go GRPC生成: 收集Proto文件失败",
			zap.String("root_dir", rootDir),
			zap.Error(err),
		)
	}
	if len(protoFiles) == 0 {
		logger.Global.Info("Go GRPC生成: 无需要处理的Proto文件，跳过目录",
			zap.String("root_dir", rootDir),
		)
		return nil
	}

	// 2. 解析输出目录
	nodeGoDir := _config.Global.Paths.NodeGoDir
	if err := utils2.EnsureDir(nodeGoDir); err != nil {
		logger.Global.Fatal("Go GRPC生成: 创建输出目录失败",
			zap.String("output_dir", nodeGoDir),
			zap.Error(err),
		)
	}

	// 3. 解析Proto根路径
	protoRootPath := filepath.Dir(filepath.Clean(rootDir))

	// 4. 生成Go GRPC代码
	if err := GenerateGoGrpc(protoFiles, nodeGoDir, protoRootPath); err != nil {
		logger.Global.Fatal("Go GRPC生成: 代码生成失败",
			zap.String("root_dir", rootDir),
			zap.String("output_dir", nodeGoDir),
			zap.Error(err),
		)
	}

	logger.Global.Info("Go GRPC生成: 目录处理完成",
		zap.String("root_dir", rootDir),
		zap.Int("proto_file_count", len(protoFiles)),
	)
	return nil
}

// GenerateRobotGoProto 递归处理目录下Proto文件，生成Go GRPC代码
func GenerateRobotGoProto(rootDir string, protoRootPath string) error {
	// 1. 收集非数据库Proto文件
	protoFiles, err := collectGoGrpcProtoFiles(rootDir)
	if err != nil {
		logger.Global.Fatal("Go GRPC生成: 收集Proto文件失败",
			zap.String("root_dir", rootDir),
			zap.Error(err),
		)
	}
	if len(protoFiles) == 0 {
		logger.Global.Info("Go GRPC生成: 无需要处理的Proto文件，跳过目录",
			zap.String("root_dir", rootDir),
		)
		return nil
	}

	// 2. 解析输出目录
	nodeGoDir := _config.Global.Paths.ToolsDir
	if err := utils2.EnsureDir(nodeGoDir); err != nil {
		logger.Global.Fatal("Go GRPC生成: 创建输出目录失败",
			zap.String("output_dir", nodeGoDir),
			zap.Error(err),
		)
	}

	// 4. 生成Go GRPC代码
	if err := GenerateGoGrpc(protoFiles, nodeGoDir, protoRootPath); err != nil {
		logger.Global.Fatal("Go GRPC生成: 代码生成失败",
			zap.String("root_dir", rootDir),
			zap.String("output_dir", nodeGoDir),
			zap.Error(err),
		)
	}

	logger.Global.Info("Robot Go GRPC生成: 目录处理完成",
		zap.String("root_dir", rootDir),
		zap.Int("proto_file_count", len(protoFiles)),
	)
	return nil
}

// collectGoGrpcProtoFiles 递归收集目录下所有非数据库Proto文件
func collectGoGrpcProtoFiles(rootDir string) ([]string, error) {
	var protoFiles []string
	err := filepath.WalkDir(rootDir, func(path string, d os.DirEntry, err error) error {
		if err != nil {
			logger.Global.Fatal("Go GRPC收集: 访问路径失败",
				zap.String("path", path),
				zap.Error(err),
			)
		}

		// 跳过目录、非Proto文件和数据库专用Proto文件
		if d.IsDir() || !utils2.IsProtoFile(d) || d.Name() == _config.Global.Naming.DbProtoFile {
			return nil
		}

		// 收集符合条件的Proto文件
		absPath, err := filepath.Abs(path)
		if err != nil {
			logger.Global.Warn("Go GRPC收集: 获取绝对路径失败，跳过文件",
				zap.String("path", path),
				zap.Error(err),
			)
			return nil
		}
		protoFiles = append(protoFiles, filepath.ToSlash(absPath))
		return nil
	})

	logger.Global.Debug("Go GRPC收集: 完成文件扫描",
		zap.String("root_dir", rootDir),
		zap.Int("collected_count", len(protoFiles)),
	)
	return protoFiles, err
}

// GenerateGoGrpc 生成Go GRPC代码
func GenerateGoGrpc(protoFiles []string, outputDir string, protoRootPath string) error {
	// 提前校验空输入
	if len(protoFiles) == 0 {
		logger.Global.Fatal("Go GRPC生成: 无需要处理的Proto文件",
			zap.String("output_dir", outputDir),
		)
	}

	// 1. 解析路径
	goOutputDir := outputDir
	protoRootDir := protoRootPath
	protoBufferDir := _config.Global.Paths.ProtobufDir

	// 2. 验证protoc路径
	protocPath, err := resolveProtocPath()
	if err != nil {
		logger.Global.Fatal("Go GRPC生成: 解析protoc路径失败",
			zap.Error(err),
		)
	}

	// 3. 构建protoc参数
	args := []string{
		"--go_out=" + goOutputDir,
		"--go-grpc_out=" + goOutputDir,
		"--proto_path=" + protoRootDir,
		"--proto_path=" + protoBufferDir,
	}

	// 4. 补充Proto文件路径
	for _, file := range protoFiles {
		args = append(args, file)
	}

	logger.Global.Debug("Go GRPC生成: 执行protoc命令",
		zap.String("protoc_path", protocPath),
		zap.Strings("args", args),
		zap.Int("proto_count", len(protoFiles)),
	)

	// 5. 执行命令
	return utils2.RunProtocWithPath(protocPath, args, "生成Go GRPC代码")
}

// resolveProtocPath 解析protoc可执行文件路径
func resolveProtocPath() (string, error) {
	return "protoc", nil
}

// AddGoPackageToProtoDir 为Proto文件添加go_package声明
func AddGoPackageToProtoDir(wg *sync.WaitGroup) {
	wg.Add(1) // 补充wg计数，确保主流程等待该goroutine完成
	go func() {
		defer wg.Done()
		grpcDirs := utils2.GetGRPCSubdirectoryNames()

		// 处理普通生成目录
		for _, dirName := range grpcDirs {
			destDir := filepath.ToSlash(_config.Global.Paths.GeneratorProtoDir + dirName + "/" + _config.Global.DirectoryNames.ProtoDirName)
			baseGoPackage := filepath.ToSlash(dirName)

			if err := addDynamicGoPackage(destDir, baseGoPackage, destDir, false); err != nil {
				logger.Global.Warn("GoPackage设置: 目录处理失败",
					zap.String("dest_dir", destDir),
					zap.String("base_go_package", baseGoPackage),
					zap.Error(err),
				)
			} else {
				logger.Global.Info("GoPackage设置: 目录处理完成",
					zap.String("dest_dir", destDir),
					zap.String("base_go_package", baseGoPackage),
				)
			}
		}

		// 处理GoZero生成目录
		for _, dirName := range grpcDirs {
			destDir := filepath.ToSlash(_config.Global.Paths.GeneratorProtoDir + dirName + "/" + _config.Global.DirectoryNames.GoZeroProtoDirName)
			baseGoPackage := filepath.ToSlash(dirName)

			if err := AddGoZeroPackageToProtos(destDir, baseGoPackage, destDir, true); err != nil {
				logger.Global.Warn("GoZero GoPackage设置: 目录处理失败",
					zap.String("dest_dir", destDir),
					zap.String("base_go_package", baseGoPackage),
					zap.Error(err),
				)
			} else {
				logger.Global.Info("GoZero GoPackage设置: 目录处理完成",
					zap.String("dest_dir", destDir),
					zap.String("base_go_package", baseGoPackage),
				)
			}
		}

		// 处理RobotGeneratedProtoDir
		destDir := _config.Global.Paths.RobotGeneratedProto
		baseGoPackage := filepath.ToSlash(_config.Global.Naming.GoRobotPackage)

		if err := addDynamicGoPackage(_config.Global.Paths.RobotGeneratedProto, baseGoPackage, destDir, false); err != nil {
			logger.Global.Warn("GoPackage设置: 目录处理失败",
				zap.String("dest_dir", destDir),
				zap.String("base_go_package", baseGoPackage),
				zap.Error(err),
			)
		} else {
			logger.Global.Info("GoPackage设置: 目录处理完成",
				zap.String("dest_dir", destDir),
				zap.String("base_go_package", baseGoPackage),
			)
		}
	}()
}

// addDynamicGoPackage 为目录下所有文件生成基于相对路径的go_package
func addDynamicGoPackage(rootDir, baseGoPackage, currentDir string, isMulti bool) error {
	entries, err := os.ReadDir(currentDir)
	if err != nil {
		logger.Global.Fatal("GoPackage设置: 读取目录失败",
			zap.String("current_dir", currentDir),
			zap.Error(err),
		)
	}

	for _, entry := range entries {
		fullPath := filepath.Join(currentDir, entry.Name())
		info, err := entry.Info()
		if err != nil {
			logger.Global.Fatal("GoPackage设置: 获取文件信息失败",
				zap.String("file_path", fullPath),
				zap.Error(err),
			)
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
		logger.Global.Fatal("GoZero GoPackage设置: 读取目录失败",
			zap.String("current_dir", currentDir),
			zap.Error(err),
		)
	}

	for _, entry := range entries {
		fullPath := filepath.Join(currentDir, entry.Name())
		info, err := entry.Info()
		if err != nil {
			logger.Global.Fatal("GoZero GoPackage设置: 获取文件信息失败",
				zap.String("file_path", fullPath),
				zap.Error(err),
			)
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
		logger.Global.Fatal("GoPackage设置: 计算相对路径失败",
			zap.String("root_dir", rootDir),
			zap.String("file_path", filePath),
			zap.Error(err),
		)
	}

	// 生成go_package路径
	var goPackagePath string
	if relativePath == "." {
		// 文件在根目录
		if isGoZero {
			goPackagePath = baseGoPackage
		} else {
			goPackagePath = filepath.Join(baseGoPackage, _config.Global.DirectoryNames.ProtoDirName)
		}
	} else {
		// 拼接基础路径和相对目录
		if isGoZero {
			goPackagePath = filepath.Join(baseGoPackage, filepath.ToSlash(relativePath))
		} else {
			goPackagePath = filepath.Join(
				baseGoPackage,
				_config.Global.DirectoryNames.ProtoDirName,
				filepath.ToSlash(relativePath),
			)
		}
	}
	goPackagePath = filepath.ToSlash(goPackagePath)

	// 添加go_package到文件
	added, err := AddGoPackage(filePath, goPackagePath, isMulti)
	if err != nil {
		logger.Global.Fatal("GoPackage设置: 设置go_package失败",
			zap.String("file_path", filePath),
			zap.String("go_package", goPackagePath),
			zap.Error(err),
		)
	}

	if added {
		logger.Global.Info("GoPackage设置: 成功添加go_package",
			zap.String("file_path", filePath),
			zap.String("go_package", goPackagePath),
		)
	} else {
		logger.Global.Debug("GoPackage设置: 文件已存在go_package，跳过",
			zap.String("file_path", filePath),
			zap.String("go_package", goPackagePath),
		)
	}
	return nil
}

// BuildGrpcServiceProto 并发处理所有GRPC目录
func BuildGrpcServiceProto(wg *sync.WaitGroup) {
	grpcDirs := utils2.GetGRPCSubdirectoryNames()
	for _, dirName := range grpcDirs {
		wg.Add(1)

		// 传递当前目录名副本到goroutine，避免循环变量捕获问题
		go func(currentDir string) {
			defer wg.Done()
			destDir := _config.Global.Paths.GeneratorProtoDir + currentDir + "/" + _config.Global.DirectoryNames.NormalGoProto
			if err := GenerateGoProto(destDir); err != nil {
				logger.Global.Warn("GRPC服务构建: 目录处理失败",
					zap.String("current_dir", currentDir),
					zap.String("dest_dir", destDir),
					zap.Error(err),
				)
			} else {
				logger.Global.Info("GRPC服务构建: 目录处理完成",
					zap.String("current_dir", currentDir),
					zap.String("dest_dir", destDir),
				)
			}
		}(dirName)
	}

	wg.Add(1)

	// 传递当前目录名副本到goroutine，避免循环变量捕获问题
	go func() {
		defer wg.Done()
		robotProtoDir := _config.Global.Paths.RobotGeneratedProto
		robotRootDir := _config.Global.Paths.RobotGenerated
		if err := GenerateRobotGoProto(robotProtoDir, robotRootDir); err != nil {
			logger.Global.Warn("GRPC服务构建: Robot目录处理失败",
				zap.String("robot_proto_dir", robotProtoDir),
				zap.String("robot_root_dir", robotRootDir),
				zap.Error(err),
			)
		} else {
			logger.Global.Info("GRPC服务构建: Robot目录处理完成",
				zap.String("robot_proto_dir", robotProtoDir),
				zap.String("robot_root_dir", robotRootDir),
			)
		}
	}()
}
