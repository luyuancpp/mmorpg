package prototools

import (
	"bytes"
	"os"
	"os/exec"
	"path/filepath"
	"sync"

	"go.uber.org/zap" // 引入zap结构化日志字段
	"pbgen/global_value"
	_config "pbgen/internal/config"
	utils2 "pbgen/internal/utils"
	"pbgen/logger" // 引入全局logger包
)

// GenerateAllInOneDescriptor 生成合并的Protobuf描述符文件
func GenerateAllInOneDescriptor(wg *sync.WaitGroup) {
	wg.Add(1)

	go func() {
		defer wg.Done()
		if err := generateAllInOneDesc(); err != nil {
			logger.Global.Warn("描述符生成失败",
				zap.Error(err),
			)
		}
	}()
}

// generateAllInOneDesc 生成并解析全量描述符文件
func generateAllInOneDesc() error {
	// 步骤1：收集目标proto文件（去重）
	protoFiles, err := collectUniqueProtoFiles()
	if err != nil {
		logger.Global.Fatal("收集Proto文件失败",
			zap.Error(err),
		)
	}
	if len(protoFiles) == 0 {
		logger.Global.Info("描述符生成: 未找到任何Proto文件")
		return nil
	}
	logger.Global.Info("描述符生成: 收集到唯一Proto文件",
		zap.Int("count", len(protoFiles)),
	)

	// 步骤2：构建protoc命令参数
	args, err := buildDescriptorArgs(protoFiles)
	if err != nil {
		logger.Global.Fatal("构建命令参数失败",
			zap.Error(err),
		)
	}

	// 步骤3：执行protoc命令
	if err := executeDescriptorCommand(args); err != nil {
		logger.Global.Fatal("执行protoc失败",
			zap.Error(err),
		)
	}

	// 步骤4：读取并解析描述符文件
	if err := parseDescriptorFile(); err != nil {
		logger.Global.Fatal("解析描述符文件失败",
			zap.Error(err),
		)
	}

	logger.Global.Info("描述符生成成功",
		zap.String("output_path", _config.Global.Paths.AllInOneDesc),
	)
	return nil
}

// collectUniqueProtoFiles 收集所有唯一的Proto文件
func collectUniqueProtoFiles() ([]string, error) {
	protoFileSet := make(map[string]struct{}) // 用于去重
	var allProtoFiles []string

	for _, dir := range global_value.ProtoDirs {
		fileEntries, err := os.ReadDir(dir)
		if err != nil {
			logger.Global.Warn("描述符生成: 读取目录失败，跳过",
				zap.String("dir", dir),
				zap.Error(err),
			)
			continue
		}

		for _, entry := range fileEntries {
			if utils2.IsProtoFile(entry) {
				absPath, err := filepath.Abs(filepath.Join(dir, entry.Name()))
				if err != nil {
					logger.Global.Warn("描述符生成: 获取文件绝对路径失败，跳过",
						zap.String("file_name", entry.Name()),
						zap.String("dir", dir),
						zap.Error(err),
					)
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
	descOutput := filepath.ToSlash(_config.Global.Paths.AllInOneDesc)

	// 基础选项
	args := []string{
		"--descriptor_set_out=" + descOutput,
		"--include_imports",     // 包含所有依赖的描述符
		"--include_source_info", // 包含源码信息，便于调试
	}

	// 添加导入路径
	importPaths := []string{
		_config.Global.Paths.OutputRoot,
		_config.Global.Paths.ProtobufDir,
	}
	for _, ip := range importPaths {
		if ip == "" {
			continue // 跳过空路径
		}
		args = append(args, "--proto_path="+filepath.ToSlash(ip))
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

	logger.Global.Debug("描述符生成: 执行protoc命令",
		zap.String("protoc_path", protocPath),
		zap.Strings("args", args),
	)
	if err := cmd.Run(); err != nil {
		logger.Global.Fatal("命令执行失败",
			zap.Error(err),
			zap.String("stderr", stderr.String()),
		)
	}

	if stdout.Len() > 0 {
		logger.Global.Info("描述符生成: 命令输出",
			zap.String("stdout", stdout.String()),
		)
	}
	return nil
}
