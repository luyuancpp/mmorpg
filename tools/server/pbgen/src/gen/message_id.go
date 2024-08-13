package gen

import (
	"bufio"
	"fmt"
	"log"
	"os"
	"pbgen/config"
	"strings"
	"sync"
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
		return nil, fmt.Errorf("error opening file: %w", err)
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
		consts = append(consts, fmt.Sprintf("const %sMsgId = %s", constName, number))
	}

	if err := scanner.Err(); err != nil {
		return nil, fmt.Errorf("error reading file: %w", err)
	}

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
		return fmt.Errorf("error creating file: %w", err)
	}
	defer file.Close()

	writer := bufio.NewWriter(file)

	_, err = writer.WriteString("package main\n\n")
	if err != nil {
		return fmt.Errorf("error writing to file: %w", err)
	}

	for _, c := range cw.Constants {
		_, err := writer.WriteString(c + "\n")
		if err != nil {
			return fmt.Errorf("error writing to file: %w", err)
		}
	}

	err = writer.Flush()
	if err != nil {
		return fmt.Errorf("error flushing to file: %w", err)
	}

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
				log.Printf("error writing to file %s: %v", path, err)
			}
		}(path)
	}

	wg.Wait()
}

func WriteGoMessageId() {
	// Initialize the ConstantsGenerator
	g := NewConstantsGenerator(config.ServiceIdFilePath)
	consts, err := g.Generate()
	if err != nil {
		log.Fatal(err)
	}

	// Define file paths where constants will be written
	filePaths := []string{
		config.ClientMessageIdFile,
		config.LoginMessageIdGo,
	}

	// Write constants to files concurrently
	WriteToFiles(consts, filePaths)
}
