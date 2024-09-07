package gen

import (
	"bufio"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"pbgen/config"
	"strings"
	"text/template"
)

const handlerTemplate = `package handler

import (
	"robot/pb/game"
	"robot/pkg"
)

func {{.HandlerName}}(client *pkg.GameClient, response *game.{{.ResponseType}}) {

}
`

type ServiceData struct {
	HandlerName  string
	ResponseType string
}

// GoRobotHandlerGenerator generates Go handler files and removes obsolete ones.
func GoRobotHandlerGenerator() {
	// Track the handlers that should exist based on the current service mappings
	existingHandlers := make(map[string]bool)

	// Open the file containing the service mappings
	file, err := os.Open(config.ServiceIdFilePath)
	if err != nil {
		log.Fatalf("Error opening file: %v", err)
		return
	}
	defer file.Close()

	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		line := scanner.Text()
		if line == "" {
			continue
		}

		// Parse each line: 0=ServiceName
		parts := strings.Split(line, "=")
		if len(parts) != 2 {
			log.Printf("Invalid format: %s", line)
			continue
		}

		serviceNumber := parts[0]
		serviceName := parts[1]

		// Generate the Go handler function name and response type
		handlerName := fmt.Sprintf("%sHandler", serviceName)
		responseType := fmt.Sprintf("%sS2C", serviceName)

		// Generate a valid file name for the Go file
		fileName := sanitizeFileName(serviceName)
		filePath := filepath.Join(config.RobotMethodHandlerDirectory, fileName+".go")

		// Mark this handler as existing
		existingHandlers[filePath] = true

		// Check if the file already exists
		if fileExists(filePath) {
			fmt.Printf("File %s already exists. Skipping generation.\n", filePath)
			continue
		}

		// Create a file for the service
		err := generateHandlerFile(filePath, handlerName, responseType)
		if err != nil {
			log.Printf("Error generating file for service %s: %v", serviceNumber, err)
			continue
		}
		fmt.Printf("Generated %s for service %s\n", filePath, serviceNumber)
	}

	if err := scanner.Err(); err != nil {
		log.Fatalf("Error reading file: %v", err)
	}

	// Remove any existing handlers that are no longer part of the service list
	err = removeObsoleteHandlers(existingHandlers)
	if err != nil {
		log.Fatalf("Error removing obsolete handlers: %v", err)
	}
}

// Generates a Go file using the provided handler and response names.
func generateHandlerFile(fileName, handlerName, responseType string) error {
	// Create the file
	file, err := os.Create(fileName)
	if err != nil {
		return fmt.Errorf("could not create file: %w", err)
	}
	defer file.Close()

	// Template for the Go file content
	tmpl, err := template.New("handler").Parse(config.RobotMethodHandlerDirectory)
	if err != nil {
		return fmt.Errorf("could not parse template: %w", err)
	}

	// Fill the template with handler name and response type
	data := ServiceData{
		HandlerName:  handlerName,
		ResponseType: responseType,
	}

	// Write the content to the file
	return tmpl.Execute(file, data)
}

// Checks if a file already exists.
func fileExists(filePath string) bool {
	_, err := os.Stat(filePath)
	return !os.IsNotExist(err)
}

// sanitizeFileName replaces invalid characters in service names for valid file names.
func sanitizeFileName(serviceName string) string {
	// Replace any invalid characters (optional: you can customize this if needed)
	return strings.ReplaceAll(serviceName, " ", "_")
}

// removeObsoleteHandlers deletes files from the output directory that are not in the existing handlers map.
func removeObsoleteHandlers(existingHandlers map[string]bool) error {
	// List all files in the output directory
	files, err := os.ReadDir(config.RobotMethodHandlerDirectory)
	if err != nil {
		return fmt.Errorf("could not list directory: %w", err)
	}

	// Loop through the files in the directory
	for _, file := range files {
		// Get the full file path
		filePath := filepath.Join(config.RobotDirectory, file.Name())

		// If the file is not in the map of existing handlers, remove it
		if !existingHandlers[filePath] {
			fmt.Printf("Removing obsolete handler: %s\n", filePath)
			err := os.Remove(filePath)
			if err != nil {
				return fmt.Errorf("could not remove file %s: %w", filePath, err)
			}
		}
	}
	return nil
}
