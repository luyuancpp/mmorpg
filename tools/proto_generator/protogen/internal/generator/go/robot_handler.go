package _go

import (
	"os"
	"path/filepath"
	"strings"
	"sync"
	"text/template"

	"protogen/internal"
	_config "protogen/internal/config"
	"protogen/logger"

	"go.uber.org/zap"
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
func GoRobotHandlerGenerator(wg *sync.WaitGroup) {
	logger.Global.Info("Generating robot individual handler files",
		zap.Int("total_service_count", len(internal.GlobalRPCServiceList)),
	)

	for _, service := range internal.GlobalRPCServiceList {
		wg.Add(1)
		go func(methods internal.RPCMethods) {
			defer wg.Done()

			if !isClientMethodRepliedHandler(&methods) {
				logger.Global.Debug("Skipping non-client-response service handler generation",
					zap.Int("method_count", len(methods)),
				)
				return
			}

			for _, method := range methods {
				serviceName := method.Service()

				if !isRelevantService(method) {
					logger.Global.Debug("Skipping irrelevant service method handler generation",
						zap.String("service_name", serviceName),
						zap.String("method_name", method.Method()),
					)
					continue
				}

				handlerName := serviceName + method.Method() + "Handler"
				responseType := method.GoResponse()

				if strings.Contains(responseType, _config.Global.Naming.EmptyResponse) {
					responseType = method.GoRequest()
					logger.Global.Debug("Response type is empty, substituting request type",
						zap.String("service_name", serviceName),
						zap.String("method_name", method.Method()),
						zap.String("original_response_type", method.GoResponse()),
						zap.String("final_response_type", responseType),
					)
				}

				fileName := sanitizeFileName(serviceName + method.Method())
				filePath := filepath.Join(_config.Global.PathLists.MethodHandlerDirectories.Robot, fileName+".go")

				if fileExists(filePath) {
					logger.Global.Debug("Handler file already exists, skipping",
						zap.String("file_path", filePath),
						zap.String("service_name", serviceName),
						zap.String("method_name", method.Method()),
					)
					continue
				}

				err := generateHandlerFile(filePath, handlerName, responseType)
				if err != nil {
					logger.Global.Warn("Failed to generate handler file, skipping",
						zap.String("file_path", filePath),
						zap.String("service_name", serviceName),
						zap.String("method_name", method.Method()),
						zap.Error(err),
					)
					continue
				}

				logger.Global.Info("Handler file generated",
					zap.String("file_path", filePath),
					zap.String("handler_name", handlerName),
					zap.String("response_type", responseType),
				)
			}
		}(service.Methods)
	}

	logger.Global.Info("Robot individual handler file generation tasks submitted",
		zap.Int("service_count", len(internal.GlobalRPCServiceList)),
	)
}

// generateHandlerFile Generates a Go file using the provided handler and response names.
func generateHandlerFile(fileName, handlerName, responseType string) error {
	if fileName == "" {
		logger.Global.Fatal("Failed to generate handler file: file path is empty")
	}

	file, err := os.Create(fileName)
	if err != nil {
		logger.Global.Fatal("Failed to create handler file",
			zap.String("file_name", fileName),
			zap.Error(err),
		)
	}
	defer file.Close()

	tmpl, err := template.New("handler").Parse(handlerTemplate)
	if err != nil {
		logger.Global.Fatal("Failed to parse handler template",
			zap.String("file_name", fileName),
			zap.Error(err),
		)
	}

	data := ServiceData{
		HandlerName:  handlerName,
		ResponseType: responseType,
	}

	logger.Global.Debug("Executing handler template rendering",
		zap.String("file_name", fileName),
		zap.String("handler_name", handlerName),
		zap.String("response_type", responseType),
	)

	return tmpl.Execute(file, data)
}

// fileExists Checks if a file already exists.
func fileExists(filePath string) bool {
	_, err := os.Stat(filePath)
	if err != nil {
		if os.IsNotExist(err) {
			return false
		}
		logger.Global.Warn("Error checking file existence",
			zap.String("file_path", filePath),
			zap.Error(err),
		)
		return true
	}
	return true
}

// sanitizeFileName replaces invalid characters in service names for valid file names.
func sanitizeFileName(serviceName string) string {
	sanitized := strings.ToLower(strings.ReplaceAll(serviceName, " ", "_"))
	logger.Global.Debug("File name sanitized",
		zap.String("original_name", serviceName),
		zap.String("sanitized_name", sanitized),
	)
	return sanitized
}
