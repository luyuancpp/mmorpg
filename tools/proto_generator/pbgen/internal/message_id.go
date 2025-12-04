package internal

import (
	"bufio"
	"os"
	"path"
	"strings"
	"sync"

	"go.uber.org/zap" // 引入zap结构化日志字段
	"pbgen/global_value"
	_config "pbgen/internal/config"
	"pbgen/internal/utils"
	"pbgen/logger" // 引入全局logger包
)

// ConstantsGenerator is responsible for generating Go constants from a file.
type ConstantsGenerator struct {
	FileName string
}

// NewConstantsGenerator creates a new ConstantsGenerator with the given file name.
func NewConstantsGenerator(fileName string) *ConstantsGenerator {
	return &ConstantsGenerator{FileName: fileName}
}

// Generate generates constants from the file and returns them as a slice of strings.
func (cg *ConstantsGenerator) Generate() ([]string, error) {
	file, err := os.Open(cg.FileName)
	if err != nil {
		logger.Global.Fatal("打开常量生成源文件失败",
			zap.String("file_name", cg.FileName),
			zap.Error(err),
		)
	}
	defer file.Close()

	var consts []string
	scanner := bufio.NewScanner(file)

	for scanner.Scan() {
		line := scanner.Text()
		parts := strings.SplitN(line, "=", 2)
		if len(parts) != 2 {
			continue
		}
		number := strings.TrimSpace(parts[0])
		name := strings.TrimSpace(parts[1])

		constName := convertToValidIdentifier(name)
		consts = append(consts, "const "+constName+_config.Global.Naming.MessageId+" = "+number)
	}

	if err := scanner.Err(); err != nil {
		logger.Global.Fatal("读取常量生成源文件失败",
			zap.String("file_name", cg.FileName),
			zap.Error(err),
		)
	}

	logger.Global.Info("常量生成完成",
		zap.String("file_name", cg.FileName),
		zap.Int("const_count", len(consts)),
	)

	return consts, nil
}

// convertToValidIdentifier converts a string to a valid Go identifier.
func convertToValidIdentifier(name string) string {
	constName := strings.ReplaceAll(name, " ", "_")
	constName = strings.ReplaceAll(constName, "-", "_")
	constName = strings.ReplaceAll(constName, "/", "_")
	constName = strings.ReplaceAll(constName, ".", "_")
	constName = strings.ReplaceAll(constName, "__", "_") // Remove double underscores

	if len(constName) > 0 && (constName[0] < 'A' || constName[0] > 'Z') && (constName[0] < 'a' || constName[0] > 'z') {
		constName = "Const_" + constName
	}
	return constName
}

// ConstantsWriter is responsible for writing Go constants to a file.
type ConstantsWriter struct {
	Constants []string
	FilePath  string
}

// NewConstantsWriter creates a new ConstantsWriter with the given constants and file path.
func NewConstantsWriter(constants []string, filePath string) *ConstantsWriter {
	return &ConstantsWriter{Constants: constants, FilePath: filePath}
}

// Write writes the constants to the specified file path.
func (cw *ConstantsWriter) Write() error {
	file, err := os.Create(cw.FilePath)
	if err != nil {
		logger.Global.Fatal("创建常量文件失败",
			zap.String("file_path", cw.FilePath),
			zap.Error(err),
		)
	}
	defer file.Close()

	writer := bufio.NewWriter(file)

	_, err = writer.WriteString("package game\n\n")
	if err != nil {
		logger.Global.Fatal("写入常量文件包名失败",
			zap.String("file_path", cw.FilePath),
			zap.Error(err),
		)
	}

	for _, c := range cw.Constants {
		_, err := writer.WriteString(c + "\n")
		if err != nil {
			logger.Global.Fatal("写入常量到文件失败",
				zap.String("file_path", cw.FilePath),
				zap.String("constant", c),
				zap.Error(err),
			)
		}
	}

	err = writer.Flush()
	if err != nil {
		logger.Global.Fatal("刷写常量文件缓冲区失败",
			zap.String("file_path", cw.FilePath),
			zap.Error(err),
		)
	}

	logger.Global.Info("常量文件写入成功",
		zap.String("file_path", cw.FilePath),
		zap.Int("const_count", len(cw.Constants)),
	)

	return nil
}

// WriteToFiles concurrently writes constants to multiple files.
func WriteToFiles(constants []string, filePaths []string) {
	var wg sync.WaitGroup

	logger.Global.Info("开始并发写入常量文件",
		zap.Int("file_count", len(filePaths)),
		zap.Int("const_count", len(constants)),
	)

	for _, path := range filePaths {
		wg.Add(1)
		go func(path string) {
			defer wg.Done()
			writer := NewConstantsWriter(constants, path)
			if err := writer.Write(); err != nil {
				logger.Global.Warn("写入常量文件失败",
					zap.String("file_path", path),
					zap.Error(err),
				)
			}
		}(path)
	}

	wg.Wait()

	logger.Global.Info("所有常量文件写入完成",
		zap.Int("file_count", len(filePaths)),
	)
}

func WriteGoMessageId(wg *sync.WaitGroup) {
	wg.Add(1)
	go func() {
		defer wg.Done()

		// Initialize the ConstantsGenerator
		g := NewConstantsGenerator(_config.Global.Paths.ServiceIdFile)
		logger.Global.Info("开始生成Go MessageId常量",
			zap.String("source_file", _config.Global.Paths.ServiceIdFile),
		)

		consts, err := g.Generate()
		if err != nil {
			logger.Global.Fatal("生成MessageId常量失败",
				zap.Error(err),
			)
		}

		// Define file paths where constants will be written
		filePaths := []string{
			_config.Global.Paths.RobotMessageIdFile,
		}

		for i := 0; i < len(global_value.ProtoDirs); i++ {
			if !utils.HasGrpcService(global_value.ProtoDirs[i]) {
				logger.Global.Debug("目录无GRPC服务，跳过MessageId文件生成",
					zap.String("dir", global_value.ProtoDirs[i]),
				)
				continue
			}
			basePath := strings.ToLower(path.Base(global_value.ProtoDirs[i]))
			outputDir := _config.Global.Paths.NodeGoDir + basePath + "/" + _config.Global.Naming.GoPackage
			filePath := outputDir + "/" + _config.Global.FileExtensions.MessageIdGoFile
			filePaths = append(filePaths, filePath)

			logger.Global.Debug("添加MessageId文件输出路径",
				zap.String("dir", global_value.ProtoDirs[i]),
				zap.String("output_path", filePath),
			)
		}

		// Write constants to files concurrently
		WriteToFiles(consts, filePaths)
	}()
}
