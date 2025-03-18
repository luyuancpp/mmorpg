package gen

import (
	"fmt"
	"os"
	"path/filepath"
	"pbgen/config"
	"pbgen/util"
	"strings"
	"text/template"
)

const handlerTemplate = `package handler

import (
	"robot/pb/game"
	"robot/logic/gameobject"
)

func {{.HandlerName}}(player *gameobject.Player, response *game.{{.ResponseType}}) {
}
`

type ServiceData struct {
	HandlerName  string
	ResponseType string
}

// GoRobotHandlerGenerator generates Go handler files and removes obsolete ones.
func GoRobotHandlerGenerator() {
	// Track the handlers that should exist based on the current service mappings

	for _, v := range ServiceMethodMap {
		util.Wg.Add(1)
		go func() {
			defer util.Wg.Done()
			if !isClientMethodRepliedHandler(&v) {
				return
			}

			for _, method := range v {
				serviceName := method.Service

				if !isRelevantService(method) {
					continue
				}
				// Generate the Go handler function name and response type
				handlerName := fmt.Sprintf("%sHandler", serviceName+method.Method)
				responseType := method.Response

				if strings.Contains(responseType, config.EmptyResponseName) {
					responseType = method.Request
				}

				// Generate a valid file name for the Go file
				fileName := sanitizeFileName(serviceName + method.Method)
				filePath := filepath.Join(config.RobotMethodHandlerDirectory, fileName+".go")

				// Check if the file already exists
				if fileExists(filePath) {
					continue
				}

				// Create a file for the service
				err := generateHandlerFile(filePath, handlerName, responseType)
				if err != nil {
					continue
				}

				fmt.Printf("Generated %s for service %s\n", filePath, fileName)
			}
		}()

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
	tmpl, err := template.New("handler").Parse(handlerTemplate)
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
	// Replace any invalid characters and convert to lowercase
	return strings.ToLower(strings.ReplaceAll(serviceName, " ", "_"))
}
