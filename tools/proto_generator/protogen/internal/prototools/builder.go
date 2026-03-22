package prototools

import (
	"errors"
	"os"
	"path/filepath"
	"sync"

	_config "protogen/internal/config"
	utils2 "protogen/internal/utils"
	"protogen/logger"

	"go.uber.org/zap"
)

// resolveGameProtoPath 解析游戏核心Proto文件路径
func ResolveGameProtoPath() (string, error) {
	gameProtoPath := filepath.Join(_config.Global.Paths.GameRpcProtoPath, _config.Global.Naming.GameRpcProto)
	if _, err := os.Stat(gameProtoPath); err != nil {
		logger.Global.Error("游戏Proto文件不存在",
			zap.String("path", gameProtoPath),
			zap.Error(err),
		)
		return "", errors.Join(errors.New("游戏Proto文件不存在"), err)
	}
	return gameProtoPath, nil
}

func BuildGeneratorGoZeroProtoPath(dir string) string {
	// dir是绝对路径，直接拼接Proto子目录名
	return filepath.Join(dir, _config.Global.DirectoryNames.GoZeroProtoDirName)
}

func BuildGeneratorProtoPath(dir string) string {
	// dir是绝对路径，直接拼接Proto子目录名
	return filepath.Join(dir, filepath.ToSlash(_config.Global.DirectoryNames.NormalGoProto))
}

// CopyProtoToGenDir 拷贝Proto文件到生成目录
func CopyProtoToGenDir(wg *sync.WaitGroup) {
	wg.Add(1)

	goProtoDirs := utils2.GetGoProtoDomainNames()

	copyToDirs := []struct {
		dirBuilder func(string) string
		desc       string
	}{
		{BuildGeneratorProtoPath, "普通生成目录"},
		{BuildGeneratorGoZeroProtoPath, "GoZero生成目录"},
	}

	go func() {
		defer wg.Done()
		for _, item := range copyToDirs {
			for _, dir := range goProtoDirs {
				destDir := _config.Global.Paths.GeneratorProtoDir + item.dirBuilder(dir)
				if err := copyProtoToDir(_config.Global.Paths.ProtoDir, destDir); err != nil {
					logger.Global.Warn("Proto拷贝失败",
						zap.String("拷贝类型", item.desc),
						zap.String("目录", dir),
						zap.Error(err),
					)
				}
			}
		}
	}()

	wg.Add(1)
	go func() {
		defer wg.Done()
		destDir := _config.Global.Paths.RobotGeneratedProto
		if err := copyProtoToDir(_config.Global.Paths.ProtoDir, destDir); err != nil {
			logger.Global.Warn("Robot Proto拷贝失败",
				zap.String("目录", _config.Global.Paths.Robot),
				zap.Error(err),
			)
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
