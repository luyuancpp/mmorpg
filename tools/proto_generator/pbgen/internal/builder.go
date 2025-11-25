package internal

import (
	"errors"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"pbgen/config"
	"pbgen/generator/cpp"
	"pbgen/generator/go"
	"pbgen/internal/protohelper"
	"pbgen/utils"
	"sync"
)

// BuildProtocCpp 并发处理所有目录的C++代码生成
func BuildProtocCpp() {
	for i := 0; i < len(config.ProtoDirs); i++ {
		utils.Wg.Add(1)
		go func(dirIndex int) {
			defer utils.Wg.Done()
			dir := config.ProtoDirs[dirIndex]
			if err := cpp.BuildProtoCpp(dir); err != nil {
				log.Printf("C++批量构建: 目录[%s]处理失败: %v", dir, err)
			}
		}(i)

		utils.Wg.Add(1)
		go func(dirIndex int) {
			defer utils.Wg.Done()
			dir := config.ProtoDirs[dirIndex]
			if err := cpp.BuildProtoGrpcCpp(dir); err != nil {
				log.Printf("GRPC C++批量构建: 目录[%s]处理失败: %v", dir, err)
			}
		}(i)
	}
}

// BuildGrpcServiceProto 并发处理所有GRPC目录
func BuildGrpcServiceProto() {
	grpcDirs := utils.GetGRPCSubdirectoryNames()
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
		if err := _go.GenerateRobotGoProto(currentDir); err != nil {
			log.Printf("GRPC服务构建: 目录[%s]处理失败: %v", currentDir, err)
		} else {
			log.Printf("GRPC服务构建: 目录[%s]处理完成", currentDir)
		}
	}(buildGeneratorProtoPath(config.RobotDirectory))

	wg.Wait()
	log.Println("GRPC服务构建: 所有目录处理完成")
}

// processGrpcDir 处理单个GRPC目录
func processGrpcDir(dirName string) error {
	destDir := buildGeneratorProtoPath(dirName)
	if _, err := os.Stat(destDir); errors.Is(err, os.ErrNotExist) {
		return fmt.Errorf("目录[%s]不存在", destDir)
	}

	return _go.GenerateGoProto(destDir)
}

// CopyProtoToGenDir 拷贝Proto文件到生成目录
func CopyProtoToGenDir() {
	utils.Wg.Add(1)
	go func() {
		defer utils.Wg.Done()
		grpcDirs := utils.GetGRPCSubdirectoryNames()

		// 拷贝到普通生成目录
		for _, dir := range grpcDirs {
			destDir := buildGeneratorProtoPath(dir)
			if err := copyProtoToDir(dir, destDir); err != nil {
				log.Printf("Proto拷贝: 目录[%s]拷贝失败: %v", dir, err)
			}
		}

		// 拷贝到GoZero生成目录
		for _, dir := range grpcDirs {
			destDir := buildGeneratorGoZeroProtoPath(dir)
			if err := copyProtoToDir(dir, destDir); err != nil {
				log.Printf("GoZero Proto拷贝: 目录[%s]拷贝失败: %v", dir, err)
			}
		}

		destDir := buildGeneratorProtoPath(config.RobotDirectory)
		if err := copyProtoToDir(config.RobotDirectory, destDir); err != nil {
			log.Printf("GoZero Proto拷贝: 目录[%s]拷贝失败: %v", config.RobotDirectory, err)
		}
	}()
}

// copyProtoToDir 拷贝单个目录的Proto文件
func copyProtoToDir(srcDir, destDir string) error {
	if err := os.MkdirAll(destDir, 0755); err != nil {
		return fmt.Errorf("创建目录[%s]失败: %w", destDir, err)
	}

	if err := utils.CopyLocalDir(config.ProtoDir, destDir); err != nil {
		return fmt.Errorf("拷贝失败: %s -> %s: %w", config.ProtoDir, destDir, err)
	}
	return nil
}

// AddGoPackageToProtoDir 为Proto文件添加go_package声明
func AddGoPackageToProtoDir() {
	utils.Wg.Add(1)
	go func() {
		defer utils.Wg.Done()
		grpcDirs := utils.GetGRPCSubdirectoryNames()

		// 处理普通生成目录
		for _, dirName := range grpcDirs {
			destDir := buildGeneratorProtoPath(dirName)
			baseGoPackage := filepath.ToSlash(dirName)

			if err := addDynamicGoPackage(destDir, baseGoPackage, destDir, false); err != nil {
				log.Printf("GoPackage设置: 目录[%s]处理失败: %v", destDir, err)
			}
		}

		// 处理GoZero生成目录
		for _, dirName := range grpcDirs {
			destDir := buildGeneratorGoZeroProtoPath(dirName)
			baseGoPackage := filepath.ToSlash(dirName)

			if err := AddGoZeroPackageToProtos(destDir, baseGoPackage, destDir, true); err != nil {
				log.Printf("GoZero GoPackage设置: 目录[%s]处理失败: %v", destDir, err)
			}
		}

		destDir := buildGeneratorProtoPath(config.RobotDirectory)
		baseGoPackage := filepath.ToSlash(config.GoRobotPackage)

		if err := addDynamicGoPackage(destDir, baseGoPackage, destDir, false); err != nil {
			log.Printf("GoPackage设置: 目录[%s]处理失败: %v", destDir, err)
		}
	}()
}

// buildGeneratorProtoPath 构建生成器的Proto路径
func buildGeneratorProtoPath(dir string) string {
	return filepath.Join(
		config.GeneratorProtoDirectory,
		dir,
		config.ProtoDirName,
	)
}

// buildGeneratorGoZeroProtoPath 构建GoZero生成器的Proto路径
func buildGeneratorGoZeroProtoPath(dir string) string {
	return filepath.Join(
		config.GeneratorProtoDirectory,
		dir,
		config.GoZeroProtoDirName,
	)
}

// addDynamicGoPackage 为目录下所有文件生成基于相对路径的go_package
func addDynamicGoPackage(rootDir, baseGoPackage, currentDir string, isMulti bool) error {
	entries, err := os.ReadDir(currentDir)
	if err != nil {
		return fmt.Errorf("读取目录[%s]失败: %w", currentDir, err)
	}

	for _, entry := range entries {
		fullPath := filepath.Join(currentDir, entry.Name())
		info, err := entry.Info()
		if err != nil {
			return fmt.Errorf("获取文件信息[%s]失败: %w", fullPath, err)
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
		return fmt.Errorf("读取目录[%s]失败: %w", currentDir, err)
	}

	for _, entry := range entries {
		fullPath := filepath.Join(currentDir, entry.Name())
		info, err := entry.Info()
		if err != nil {
			return fmt.Errorf("获取文件信息[%s]失败: %w", fullPath, err)
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
		return fmt.Errorf("计算相对路径失败: %w", err)
	}

	// 生成go_package路径
	var goPackagePath string
	if relativePath == "." {
		// 文件在根目录
		if isGoZero {
			goPackagePath = baseGoPackage
		} else {
			goPackagePath = filepath.Join(baseGoPackage, config.ProtoDirName)
		}
	} else {
		// 拼接基础路径和相对目录
		if isGoZero {
			goPackagePath = filepath.Join(baseGoPackage, filepath.ToSlash(relativePath))
		} else {
			goPackagePath = filepath.Join(
				baseGoPackage,
				config.ProtoDirName,
				filepath.ToSlash(relativePath),
			)
		}
	}
	goPackagePath = filepath.ToSlash(goPackagePath)

	// 添加go_package到文件
	added, err := protohelper.AddGoPackage(filePath, goPackagePath, isMulti)
	if err != nil {
		return fmt.Errorf("设置go_package失败: %w", err)
	}

	if added {
		log.Printf("GoPackage设置: 文件[%s]设置为[%s]", filePath, goPackagePath)
	} else {
		log.Printf("GoPackage设置: 文件[%s]已存在，跳过", filePath)
	}
	return nil
}
