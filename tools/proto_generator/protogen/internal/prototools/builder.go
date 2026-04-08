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

// ResolveGameProtoPath resolves the path to the core game proto file.
func ResolveGameProtoPath() (string, error) {
	gameProtoPath := filepath.Join(_config.Global.Paths.GameRpcProtoPath, _config.Global.Naming.GameRpcProto)
	if _, err := os.Stat(gameProtoPath); err != nil {
		logger.Global.Error("Game proto file does not exist",
			zap.String("path", gameProtoPath),
			zap.Error(err),
		)
		return "", errors.Join(errors.New("game proto file does not exist"), err)
	}
	return gameProtoPath, nil
}

func BuildGeneratorGoZeroProtoPath(dir string) string {
	// dir is an absolute path; append the proto subdirectory name directly
	return filepath.Join(dir, _config.Global.DirectoryNames.GoZeroProtoDirName)
}

func BuildGeneratorProtoPath(dir string) string {
	// dir is an absolute path; append the proto subdirectory name directly
	return filepath.Join(dir, filepath.ToSlash(_config.Global.DirectoryNames.NormalGoProto))
}

// CopyProtoToGenDir copies proto files to the generation output directories.
func CopyProtoToGenDir(wg *sync.WaitGroup) {
	wg.Add(1)

	goProtoDirs := utils2.GetGoProtoDomainNames()

	copyToDirs := []struct {
		dirBuilder func(string) string
		desc       string
	}{
		{BuildGeneratorProtoPath, "normal generation dir"},
		{BuildGeneratorGoZeroProtoPath, "go-zero generation dir"},
	}

	go func() {
		defer wg.Done()
		for _, item := range copyToDirs {
			for _, dir := range goProtoDirs {
				destDir := _config.Global.Paths.GeneratorProtoDir + item.dirBuilder(dir)
				if err := copyProtoToDir(_config.Global.Paths.ProtoDir, destDir); err != nil {
				logger.Global.Warn("Failed to copy proto files",
					zap.String("copy_type", item.desc),
					zap.String("dir", dir),
						zap.Error(err),
					)
				}
			}
		}
	}()

	// Robot proto copy removed: robot now uses go/proto via replace directive.
}

// copyProtoToDir copies proto files from a single source directory to the destination.
func copyProtoToDir(srcDir, destDir string) error {
	if err := os.MkdirAll(destDir, 0755); err != nil {
		return errors.Join(errors.New("failed to create directory"), err)
	}

	if err := utils2.CopyLocalDir(srcDir, destDir); err != nil {
		return errors.Join(errors.New("failed to copy files"), err)
	}
	return nil
}
