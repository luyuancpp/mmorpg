package internal

import (
	"bufio"
	"os"
	"path/filepath"
	"strings"
	"sync"

	_config "protogen/internal/config"
	"protogen/internal/utils"
	"protogen/logger"

	"go.uber.org/zap"
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
	return cg.GenerateWithSuffix(_config.Global.Naming.MessageId)
}

func (cg *ConstantsGenerator) GenerateWithSuffix(suffix string) ([]string, error) {
	file, err := os.Open(cg.FileName)
	if err != nil {
		logger.Global.Fatal("Failed to open constants source file",
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
		consts = append(consts, "const "+constName+suffix+" = "+number)
	}

	if err := scanner.Err(); err != nil {
		logger.Global.Fatal("Failed to read constants source file",
			zap.String("file_name", cg.FileName),
			zap.Error(err),
		)
	}

	logger.Global.Info("Constants generated",
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
	dir := filepath.Dir(cw.FilePath)
	if err := os.MkdirAll(dir, 0755); err != nil {
		logger.Global.Fatal("Failed to create constants directory",
			zap.String("file_path", cw.FilePath),
			zap.Error(err),
		)
		return err
	}

	file, err := os.Create(cw.FilePath)
	if err != nil {
		logger.Global.Fatal("Failed to create constants file",
			zap.String("file_path", cw.FilePath),
			zap.Error(err),
		)
	}
	defer file.Close()

	writer := bufio.NewWriter(file)

	_, err = writer.WriteString("package game\n\n")
	if err != nil {
		logger.Global.Fatal("Failed to write package name to constants file",
			zap.String("file_path", cw.FilePath),
			zap.Error(err),
		)
	}

	for _, c := range cw.Constants {
		_, err := writer.WriteString(c + "\n")
		if err != nil {
			logger.Global.Fatal("Failed to write constant to file",
				zap.String("file_path", cw.FilePath),
				zap.String("constant", c),
				zap.Error(err),
			)
		}
	}

	err = writer.Flush()
	if err != nil {
		logger.Global.Fatal("Failed to flush constants file buffer",
			zap.String("file_path", cw.FilePath),
			zap.Error(err),
		)
	}

	logger.Global.Info("Constants file written",
		zap.String("file_path", cw.FilePath),
		zap.Int("const_count", len(cw.Constants)),
	)

	return nil
}

// WriteToFiles concurrently writes constants to multiple files.
func WriteToFiles(constants []string, filePaths []string) {
	var wg sync.WaitGroup
	for _, path := range filePaths {
		wg.Add(1)
		go func(path string) {
			defer wg.Done()
			writer := NewConstantsWriter(constants, path)
			if err := writer.Write(); err != nil {
				logger.Global.Warn("Failed to write constants file",
					zap.String("file_path", path),
					zap.Error(err),
				)
			}
		}(path)
	}
	wg.Wait()
}

func generateConstants(fileName, suffix string) ([]string, error) {
	g := NewConstantsGenerator(fileName)
	consts, err := g.GenerateWithSuffix(suffix)
	if err != nil {
		return nil, err
	}
	return consts, nil
}

func WriteGoMessageId(wg *sync.WaitGroup) {
	wg.Add(1)
	go func() {
		defer wg.Done()
		consts, err := generateConstants(_config.Global.Paths.ServiceIdFile, _config.Global.Naming.MessageId)
		if err != nil {
			logger.Global.Fatal("Failed to generate MessageId constants", zap.Error(err))
		}
		filePaths := []string{
			_config.Global.Paths.RobotMessageIdFile,
		}
		for domain, meta := range _config.Global.DomainMeta {
			if !utils.IsGRPC(meta) {
				logger.Global.Debug("Domain has no GRPC service, skipping MessageId file generation",
					zap.String("domain", domain),
				)
				continue
			}
			basePath := strings.ToLower(filepath.Base(meta.Source))
			outputDir := _config.Global.Paths.NodeGoDir + basePath + "/" + _config.Global.Naming.GoPackage
			filePath := outputDir + "/" + _config.Global.FileExtensions.MessageIdGoFile
			filePaths = append(filePaths, filePath)
		}
		WriteToFiles(consts, filePaths)
	}()
}

func WriteGoEventId(wg *sync.WaitGroup) {
	wg.Add(1)
	go func() {
		defer wg.Done()
		consts, err := generateConstants(_config.Global.Paths.EventIdFile, _config.Global.Naming.EventId)
		if err != nil {
			logger.Global.Fatal("Failed to generate EventId constants", zap.Error(err))
		}

		filePaths := make([]string, 0)
		for domain, meta := range _config.Global.DomainMeta {
			if !utils.IsGRPC(meta) {
				logger.Global.Debug("Domain has no GRPC service, skipping EventId file generation",
					zap.String("domain", domain),
				)
				continue
			}
			basePath := strings.ToLower(filepath.Base(meta.Source))
			outputDir := _config.Global.Paths.NodeGoDir + basePath + "/" + _config.Global.Naming.GoPackage
			filePaths = append(filePaths, outputDir+"/"+_config.Global.FileExtensions.EventIdGoFile)
		}

		WriteToFiles(consts, filePaths)
	}()
}
