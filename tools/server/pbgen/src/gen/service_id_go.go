package gen

import (
	"bufio"
	"fmt"
	"log"
	"os"
	"pbgen/config"
	"pbgen/util"
	"strings"
)

// GenerateServiceIds reads a file and generates Go const declarations.
// It expects the file to have lines in the format of "number=name".
func GenerateServiceIds(filename string) ([]string, error) {
	file, err := os.Open(filename)
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

		// Convert name to a valid Go identifier
		constName := strings.ReplaceAll(name, " ", "_")
		constName = strings.ReplaceAll(constName, "-", "_")
		constName = strings.ReplaceAll(constName, "/", "_")
		constName = strings.ReplaceAll(constName, ".", "_")
		constName = strings.ReplaceAll(constName, "__", "_") // Remove double underscores

		// Ensure the constName starts with a letter
		if len(constName) > 0 && (constName[0] < 'A' || constName[0] > 'Z') && (constName[0] < 'a' || constName[0] > 'z') {
			constName = "Const_" + constName
		}

		consts = append(consts, fmt.Sprintf("const %sMsgId = %s", constName, number))
	}

	if err := scanner.Err(); err != nil {
		return nil, fmt.Errorf("error reading file: %w", err)
	}

	return consts, nil
}

// WriteConstsToFile writes generated const declarations to a Go source file.
func WriteConstsToFile(consts []string, outputPath string) error {
	file, err := os.Create(outputPath)
	if err != nil {
		return fmt.Errorf("error creating file: %w", err)
	}
	defer file.Close()

	writer := bufio.NewWriter(file)

	_, err = writer.WriteString("package main\n\n")
	if err != nil {
		return fmt.Errorf("error writing to file: %w", err)
	}

	for _, c := range consts {
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

func WriteGoMessageId() {

	consts, err := GenerateServiceIds(config.ServiceIdsFileName)
	if err != nil {
		log.Fatal(err)
	}

	util.Wg.Add(1)
	go func(consts []string) {
		defer util.Wg.Done()
		err = WriteConstsToFile(consts, config.ClientMessageIdFile) // 替换为你的输出路径
		if err != nil {
			log.Fatal(err)
		}
	}(consts)

	util.Wg.Add(1)
	go func(consts []string) {
		defer util.Wg.Done()
		err = WriteConstsToFile(consts, config.LoginMessageIdGo) // 替换为你的输出路径
		if err != nil {
			log.Fatal(err)
		}
	}(consts)
}
