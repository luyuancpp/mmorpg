package proto

import (
	"fmt"
	"log"
	"os"
	"path/filepath"
	"pbgen/config"
	_config "pbgen/internal/config"
	utils2 "pbgen/internal/utils"
)

// resolveGameProtoPath 解析游戏核心Proto文件路径
func ResolveGameProtoPath() (string, error) {
	gameProtoRoot, err := utils2.ResolveAbsPath(_config.Global.Paths.GameRpcProtoPath, "游戏Proto根目录")
	if err != nil {
		return "", err
	}

	gameProtoPath := filepath.Join(gameProtoRoot, config.GameRpcProtoName)
	if _, err := os.Stat(gameProtoPath); err != nil {
		return "", fmt.Errorf("游戏Proto文件不存在: 路径=%s, 错误=%w", gameProtoPath, err)
	}
	return gameProtoPath, nil
}

// BuildGeneratorGoZeroProtoPath 构建GoZero生成器的Proto路径
func BuildGeneratorGoZeroProtoPath(dir string) string {
	return filepath.Join(
		config.GeneratorProtoDirectory,
		dir,
		_config.Global.FileExtensions.GoZeroProtoDirName,
	)
}

// buildGeneratorProtoPath 构建生成器的Proto路径
func BuildGeneratorProtoPath(dir string) string {
	return filepath.Join(
		config.GeneratorProtoDirectory,
		dir,
		_config.Global.FileExtensions.ProtoDirName,
	)
}

// CopyProtoToGenDir 拷贝Proto文件到生成目录
func CopyProtoToGenDir() {
	utils2.Wg.Add(1)
	go func() {
		defer utils2.Wg.Done()
		grpcDirs := utils2.GetGRPCSubdirectoryNames()

		// 拷贝到普通生成目录
		for _, dir := range grpcDirs {
			destDir := BuildGeneratorProtoPath(dir)
			if err := copyProtoToDir(dir, destDir); err != nil {
				log.Printf("Proto拷贝: 目录[%s]拷贝失败: %v", dir, err)
			}
		}

		// 拷贝到GoZero生成目录
		for _, dir := range grpcDirs {
			destDir := BuildGeneratorGoZeroProtoPath(dir)
			if err := copyProtoToDir(dir, destDir); err != nil {
				log.Printf("GoZero Proto拷贝: 目录[%s]拷贝失败: %v", dir, err)
			}
		}

		destDir := BuildGeneratorProtoPath(_config.Global.Paths.RobotDir)
		if err := copyProtoToDir(_config.Global.Paths.RobotDir, destDir); err != nil {
			log.Printf("GoZero Proto拷贝: 目录[%s]拷贝失败: %v", _config.Global.Paths.RobotDir, err)
		}
	}()
}

// copyProtoToDir 拷贝单个目录的Proto文件
func copyProtoToDir(srcDir, destDir string) error {
	if err := os.MkdirAll(destDir, 0755); err != nil {
		return fmt.Errorf("创建目录[%s]失败: %w", destDir, err)
	}

	if err := utils2.CopyLocalDir(_config.Global.Paths.ProtoDir, destDir); err != nil {
		return fmt.Errorf("拷贝失败: %s -> %s: %w", _config.Global.Paths.ProtoDir, destDir, err)
	}
	return nil
}
