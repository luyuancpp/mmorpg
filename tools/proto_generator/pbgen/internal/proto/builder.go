package proto

import (
	"errors"
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
		return "", errors.Join(errors.New("游戏Proto文件不存在"), err)
	}
	return gameProtoPath, nil
}

// BuildGeneratorGoZeroProtoPath 构建GoZero生成器的Proto路径（dir为绝对路径）
func BuildGeneratorGoZeroProtoPath(dir string) string {
	// dir是绝对路径，直接拼接Proto子目录名
	return filepath.Join(dir, _config.Global.FileExtensions.GoZeroProtoDirName)
}

// BuildGeneratorProtoPath 构建生成器的Proto路径（dir为绝对路径）
func BuildGeneratorProtoPath(dir string) string {
	// dir是绝对路径，直接拼接Proto子目录名
	return filepath.Join(dir, _config.Global.FileExtensions.ProtoDirName)
}

// CopyProtoToGenDir 拷贝Proto文件到生成目录
func CopyProtoToGenDir() {
	grpcDirs := utils2.GetGRPCSubdirectoryNames()

	// 拷贝到不同生成目录
	copyToDirs := []struct {
		dirBuilder func(string) string
		desc       string
	}{
		{BuildGeneratorProtoPath, "普通生成目录"},
		{BuildGeneratorGoZeroProtoPath, "GoZero生成目录"},
	}

	for _, item := range copyToDirs {
		for _, dir := range grpcDirs {
			utils2.Wg.Add(1)
			go func(d, desc string, builder func(string) string) {
				defer utils2.Wg.Done()
				destDir := builder(d)
				if err := copyProtoToDir(_config.Global.Paths.ProtoDir, destDir); err != nil {
					log.Printf("%s Proto拷贝: 目录[%s]拷贝失败: %v", desc, d, err)
				}
			}(dir, item.desc, item.dirBuilder)
		}
	}

	// 拷贝Robot目录（RobotDir若为绝对路径则直接使用）
	utils2.Wg.Add(1)
	go func() {
		defer utils2.Wg.Done()
		destDir := BuildGeneratorProtoPath(_config.Global.Paths.RobotDir)
		if err := copyProtoToDir(_config.Global.Paths.ProtoDir, destDir); err != nil {
			log.Printf("Robot Proto拷贝: 目录[%s]拷贝失败: %v", _config.Global.Paths.RobotDir, err)
		}
	}()
}

// copyProtoToDir 拷贝单个目录的Proto文件
func copyProtoToDir(srcDir, destDir string) error {
	if err := os.MkdirAll(destDir, 0755); err != nil {
		return errors.Join(errors.New("创建目录失败"), err)
	}

	if err := utils2.CopyLocalDir(srcDir, destDir); err != nil {
		return errors.Join(errors.New("拷贝失败"), err)
	}
	return nil
}
