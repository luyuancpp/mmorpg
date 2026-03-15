package _go

import (
	"os"
	"path/filepath"
	"strings"
	"sync"

	_config "pbgen/internal/config"
	utils2 "pbgen/internal/utils"
	"pbgen/logger"

	"go.uber.org/zap"
)

func GenerateGoProto(rootDir string) error {
	if utils2.HasEtcdService(rootDir) {
		logger.Global.Debug("跳过Etcd服务相关目录",
			zap.String("root_dir", rootDir),
		)
		return nil
	}

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

	// Avoid cross-domain overwrite races: only generate files whose go_package belongs to this domain.
	domainName := filepath.Base(filepath.Dir(filepath.Clean(rootDir)))
	protoFiles = filterProtoFilesByDomainGoPackage(protoFiles, domainName)
	if len(protoFiles) == 0 {
		logger.Global.Info("Go GRPC生成: 过滤后无需要处理的Proto文件，跳过目录",
			zap.String("root_dir", rootDir),
			zap.String("domain", domainName),
		)
		return nil
	}

	nodeGoDir := _config.Global.Paths.NodeGoDir
	if err := utils2.EnsureDir(nodeGoDir); err != nil {
		logger.Global.Fatal("Go GRPC生成: 创建输出目录失败",
			zap.String("output_dir", nodeGoDir),
			zap.Error(err),
		)
	}

	protoRootPath := filepath.Dir(filepath.Clean(rootDir))

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

func filterProtoFilesByDomainGoPackage(protoFiles []string, domainName string) []string {
	if domainName == "" {
		return protoFiles
	}

	prefix := domainName + "/"
	filtered := make([]string, 0, len(protoFiles))

	for _, protoFile := range protoFiles {
		content, err := os.ReadFile(protoFile)
		if err != nil {
			logger.Global.Warn("Go GRPC过滤: 读取Proto文件失败，按保守策略保留",
				zap.String("file", protoFile),
				zap.Error(err),
			)
			filtered = append(filtered, protoFile)
			continue
		}

		goPackage := ""
		for _, line := range strings.Split(string(content), "\n") {
			trimmed := strings.TrimSpace(line)
			if !strings.HasPrefix(trimmed, "option go_package =") {
				continue
			}
			firstQuote := strings.Index(trimmed, "\"")
			if firstQuote < 0 {
				break
			}
			rest := trimmed[firstQuote+1:]
			secondQuote := strings.Index(rest, "\"")
			if secondQuote < 0 {
				break
			}
			goPackage = rest[:secondQuote]
			if semi := strings.Index(goPackage, ";"); semi >= 0 {
				goPackage = goPackage[:semi]
			}
			break
		}

		if goPackage == "" || strings.HasPrefix(goPackage, prefix) {
			filtered = append(filtered, protoFile)
			continue
		}

		logger.Global.Debug("Go GRPC过滤: 跳过跨域go_package文件",
			zap.String("file", protoFile),
			zap.String("domain", domainName),
			zap.String("go_package", goPackage),
		)
	}

	return filtered
}

// GenerateRobotGoProto 递归处理目录下Proto文件，生成Go GRPC代码
func GenerateRobotGoProto(rootDir string, protoRootPath string) error {
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

	nodeGoDir := _config.Global.Paths.ToolsDir
	if err := utils2.EnsureDir(nodeGoDir); err != nil {
		logger.Global.Fatal("Go GRPC生成: 创建输出目录失败",
			zap.String("output_dir", nodeGoDir),
			zap.Error(err),
		)
	}

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

		if d.IsDir() || !utils2.IsProtoFile(d) || d.Name() == _config.Global.Naming.DbProtoFile {
			return nil
		}

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
	if len(protoFiles) == 0 {
		logger.Global.Fatal("Go GRPC生成: 无需要处理的Proto文件",
			zap.String("output_dir", outputDir),
		)
	}

	goOutputDir := outputDir
	protoRootDir := protoRootPath
	protoBufferDir := _config.Global.Paths.ProtobufDir

	protocPath, err := resolveProtocPath()
	if err != nil {
		logger.Global.Fatal("Go GRPC生成: 解析protoc路径失败",
			zap.Error(err),
		)
	}

	args := []string{
		"--go_out=" + goOutputDir,
		"--go-grpc_out=" + goOutputDir,
		"--proto_path=" + protoRootDir,
		"--proto_path=" + protoBufferDir,
	}

	for _, file := range protoFiles {
		args = append(args, file)
	}

	logger.Global.Debug("Go GRPC生成: 执行protoc命令",
		zap.String("protoc_path", protocPath),
		zap.Strings("args", args),
		zap.Int("proto_count", len(protoFiles)),
	)

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
			if err := addDynamicGoPackage(rootDir, baseGoPackage, fullPath, isMulti); err != nil {
				return err
			}
		} else if filepath.Ext(fullPath) == ".proto" {
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
			if err := AddGoZeroPackageToProtos(rootDir, baseGoPackage, fullPath, isMulti); err != nil {
				return err
			}
		} else if filepath.Ext(fullPath) == ".proto" {
			if err := processProtoFileForGoPackage(rootDir, baseGoPackage, fullPath, isMulti, true); err != nil {
				return err
			}
		}
	}
	return nil
}

// processProtoFileForGoPackage 为单个Proto文件设置go_package
func processProtoFileForGoPackage(rootDir, baseGoPackage, filePath string, isMulti, isGoZero bool) error {
	relativePath, err := filepath.Rel(rootDir, filepath.Dir(filePath))
	if err != nil {
		logger.Global.Fatal("GoPackage设置: 计算相对路径失败",
			zap.String("root_dir", rootDir),
			zap.String("file_path", filePath),
			zap.Error(err),
		)
	}

	var goPackagePath string
	if relativePath == "." {
		if isGoZero {
			goPackagePath = baseGoPackage
		} else {
			goPackagePath = filepath.Join(baseGoPackage, _config.Global.DirectoryNames.ProtoDirName)
		}
	} else {
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
