package gen

import (
	"fmt"
	"log"
	"os"
	"pbgen/config"
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
	tmpl, err := template.New(fileName).Parse(text)
	if err != nil {
		return fmt.Errorf("could not parse template: %w", err)
	}

	// Fill the template with handler name and response type
	data := GrpcServiceData{
		GrpcServices: grpcServices,
		FileBaseName: protoFileBaseName,
	}

	// Write the content to the file
	return tmpl.Execute(file, data)
}

func CppGrpcCallClient() {
	for _, serviceMethods := range ServiceMethodMap {
		grpcServices := make([]GrpcService, 0)

		if len(serviceMethods) <= 0 {
			continue
		}

		if !strings.Contains(serviceMethods[0].Path, config.ProtoDirectoryNames[config.CommonProtoDirIndex]) {
			continue
		}

		for _, method := range serviceMethods {

			grpcServices = generateGrpcMethod(method, grpcServices)
		}

		// Generate different handler files based on the templates
		if err := generateGrpcFile(config.CppGenGrpcDirectory+serviceMethods[0].FileBaseName()+config.GrpcHeaderExtension,
			serviceMethods[0].FileBaseName(),
			grpcServices,
			AsyncClientHeaderTemplate); err != nil {
			log.Fatal(err)
		}

	}

}
