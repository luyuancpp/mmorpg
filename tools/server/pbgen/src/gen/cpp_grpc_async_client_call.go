package gen

import (
	"fmt"
	"os"
	"strings"
	"text/template"
)

type GrpcService struct {
	Request      string
	Response     string
	Method       string
	ServiceName  string
	FileBaseName string
}

type GrpcServiceData struct {
	GrpcServices []GrpcService
	FileBaseName string
}

// generateHandlerCases creates the cases for the switch statement based on the method.
func generateGrpcMethod(method *RPCMethod, grpcServices []GrpcService) []GrpcService {
	grpcService := GrpcService{
		Request:      method.Request,
		Response:     method.Response,
		Method:       method.Method,
		ServiceName:  method.Service,
		FileBaseName: strings.ToLower(method.FileBaseName()),
	}
	grpcServices = append(grpcServices, grpcService)
	return grpcServices
}

// Generates a Go file using the provided handler and response names.
func generateGrpcFile(fileName string, protoFileBaseName string, grpcServices []GrpcService, text string) error {
	// Create the file
	file, err := os.Create(fileName)
	if err != nil {
		return fmt.Errorf("could not create file: %w", err)
	}
	defer file.Close()

	// Template for the Go file content
	tmpl, err := template.New("handler").Parse(text)
	if err != nil {
		return fmt.Errorf("could not parse template: %w", err)
	}

	// Fill the template with handler name and response type
	data := GrpcServiceData{
		GrpcServices: grpcServices,
		FileBaseName: protoFileBaseName,
	}

	if err := tmpl.Execute(file, data); err != nil {
		return fmt.Errorf("could not execute template: %w", err)
	}

	// Write the content to the file
	return tmpl.Execute(file, data)
}

func CppGrpcCallClient() {

	grpcServices := make([]GrpcService, 0)

	for _, serviceMethods := range ServiceMethodMap {

		for _, method := range serviceMethods {

			if !isRelevantService(method) {
				continue
			}

			grpcServices = generateGrpcMethod(method, grpcServices)
		}
	}

	// Generate different handler files based on the templates
	if err := generateGrpcFile("test_grpc", "hello", grpcServices, AsyncClientHeaderTemplate); err != nil {
		return
	}

	// if err := generateHandlerFile(config.ResponseHandlerFile, v, responseHandlerTemplate); err != nil {
	// 	return
	// 	}

	// if err := generateHandlerFile(config.ServerHandlerFile, generateGrpcFile, serverHandlerTemplate); err != nil {
	// 	return
	// }
}
