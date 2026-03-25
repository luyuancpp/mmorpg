package _go

import (
	"bufio"
	"fmt"
	"os"
	"strings"

	"protogen/logger"

	"go.uber.org/zap"
)

// GenGoPackageOptWithPkg generates a go_package option with path only.
// Avoids semicolon alias form ("path;pkg") which causes goctl to generate
// broken import aliases like db_db or login_login.
func GenGoPackageOptWithPkg(goPackagePath string) string {
	return fmt.Sprintf("option go_package = \"%s\";", goPackagePath)
}

func GenGoPackageOptWithAdjustedPath(goPackagePath string) string {
	parts := strings.Split(goPackagePath, "/")
	if len(parts) < 1 {
		return `option go_package = "unknown;unknown_proto";`
	}

	var remainingParts []string
	if len(parts) >= 2 {
		remainingParts = parts[1:]
	} else if len(parts) == 1 {
		remainingParts = []string{}
	} else {
		pkgName := parts[0] + "_proto"
		pkgName = strings.ReplaceAll(pkgName, "-", "_")
		pkgName = strings.ReplaceAll(pkgName, ".", "_")
		return fmt.Sprintf("option go_package = \"%s\";", goPackagePath)
	}

	fullParts := remainingParts
	newPath := strings.Join(fullParts, "/")

	return fmt.Sprintf("option go_package = \"%s\";", newPath)
}

// AddGoPackage inserts an option go_package line into a proto file if not present.
func AddGoPackage(protoFile, goPackagePath string, isMulti bool) (bool, error) {
	file, err := os.Open(protoFile)
	if err != nil {
	logger.Global.Fatal("Failed to open proto file",
		zap.String("file_path", protoFile),
			zap.Error(err),
		)
	}
	defer file.Close()

	var lines []string
	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		lines = append(lines, scanner.Text())
	}
	if err := scanner.Err(); err != nil {
	logger.Global.Fatal("Failed to read proto file content",
		zap.String("file_path", protoFile),
			zap.Error(err),
		)
	}

	// Check if go_package already exists — if so, replace it with the correct one
	for i, line := range lines {
		trimmed := strings.TrimSpace(line)
		if strings.HasPrefix(trimmed, "option go_package =") {
			var goPackageLine string
			if isMulti {
				goPackageLine = GenGoPackageOptWithAdjustedPath(goPackagePath)
			} else {
				goPackageLine = GenGoPackageOptWithPkg(goPackagePath)
			}
			if trimmed == goPackageLine {
				return false, nil // already correct
			}
			lines[i] = goPackageLine
			return writeProtoFile(protoFile, lines)
		}
	}

	insertIndex := 0
	foundSyntax := false
	foundPackage := false

	for i, line := range lines {
		trimmed := strings.TrimSpace(line)

		if strings.HasPrefix(trimmed, "syntax =") {
			foundSyntax = true
			insertIndex = i + 1 // right after syntax
			continue
		}

		if strings.HasPrefix(trimmed, "package ") {
			foundPackage = true
			insertIndex = i // insert before package
			break
		}
	}

	if !foundSyntax {
		if !foundPackage {
			insertIndex = 0
		}
	}

	var goPackageLine string
	if isMulti {
		goPackageLine = GenGoPackageOptWithAdjustedPath(goPackagePath)
	} else {
		goPackageLine = GenGoPackageOptWithPkg(goPackagePath)
	}

	newLines := make([]string, 0, len(lines)+1)
	newLines = append(newLines, lines[:insertIndex]...)
	newLines = append(newLines, goPackageLine)
	newLines = append(newLines, lines[insertIndex:]...)

	return writeProtoFile(protoFile, newLines)
}

func writeProtoFile(protoFile string, lines []string) (bool, error) {
	output, err := os.Create(protoFile)
	if err != nil {
		logger.Global.Fatal("Failed to create proto file write handle",
			zap.String("file_path", protoFile),
			zap.Error(err),
		)
	}
	defer output.Close()

	writer := bufio.NewWriter(output)
	for _, line := range lines {
		if _, err := writer.WriteString(line + "\n"); err != nil {
			logger.Global.Fatal("Failed to write proto file content",
				zap.String("file_path", protoFile),
				zap.Error(err),
			)
		}
	}
	if err := writer.Flush(); err != nil {
		logger.Global.Fatal("Failed to flush proto file write buffer",
			zap.String("file_path", protoFile),
			zap.Error(err),
		)
	}

	return true, nil
}
