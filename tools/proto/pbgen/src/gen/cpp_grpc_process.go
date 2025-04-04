package gen

import (
	"fmt"
	"log"
	"os"
	"path/filepath"
	"pbgen/config"
	"pbgen/util"
	"strings"
	"text/template"
)

type GrpcService struct {
	Request                    string
	Response                   string
	Method                     string
	ServiceName                string
	FileBaseName               string
	ServiceFullNameWithColon   string
	ServiceFullNameWithNoColon string
	PbPackageName              string
	PackageNameWithColon       string
}

type GrpcServiceTemplateData struct {
	GrpcServices               []GrpcService
	ServiceName                string
	ProtoFileBaseName          string
	GeneratorFileName          string
	IncludeName                string
	ServiceFullNameWithColon   string
	ServiceFullNameWithNoColon string
	PbPackageName              string
	PackageNameWithColon       string
}

// generateHandlerCases creates the cases for the switch statement based on the method.
func generateGrpcMethod(method *RPCMethod, grpcServices []GrpcService) []GrpcService {
	grpcService := GrpcService{
		Request:                    method.Request,
		Response:                   method.Response,
		Method:                     method.Method,
		ServiceName:                method.Service,
		FileBaseName:               strings.ToLower(method.FileBaseName()),
		ServiceFullNameWithColon:   method.GetServiceFullNameWithColon(),
		ServiceFullNameWithNoColon: method.GetServiceFullNameWithNoColon(),
		PbPackageName:              method.PbPackage,
		PackageNameWithColon:       method.GetPackageNameWithColon(),
	}
	grpcServices = append(grpcServices, grpcService)
	return grpcServices
}

// Generates a Go file using the provided handler and response names.
func generateGrpcFile(fileName string, protoFileBaseName string, grpcServices []GrpcService, text string, method *RPCMethod) error {
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
	data := GrpcServiceTemplateData{
		GrpcServices:               grpcServices,
		ServiceName:                method.Service,
		ProtoFileBaseName:          protoFileBaseName,
		GeneratorFileName:          filepath.Base(strings.TrimSuffix(fileName, filepath.Ext(fileName))),
		IncludeName:                method.GrpcIncludeHeadName(),
		ServiceFullNameWithColon:   method.GetServiceFullNameWithColon(),
		ServiceFullNameWithNoColon: method.GetServiceFullNameWithNoColon(),
		PbPackageName:              method.PbPackage,
		PackageNameWithColon:       method.GetPackageNameWithColon(),
	}

	// Write the content to the file
	return tmpl.Execute(file, data)
}

func CppGrpcCallClient() {
	for _, serviceMethods := range ServiceMethodMap {

		util.Wg.Add(1)
		go func() {
			defer util.Wg.Done()

			grpcServices := make([]GrpcService, 0)

			if len(serviceMethods) <= 0 {
				return
			}

			if !(strings.Contains(serviceMethods[0].Path, config.ProtoDirectoryNames[config.CommonProtoDirIndex]) ||
				strings.Contains(serviceMethods[0].Path, config.ProtoDirectoryNames[config.EtcdProtoDirIndex])) {
				return
			}

			for _, method := range serviceMethods {
				grpcServices = generateGrpcMethod(method, grpcServices)
			}

			method := serviceMethods[0]
			// Generate different handler files based on the templates
			if err := generateGrpcFile(config.CppGenGrpcDirectory+serviceMethods[0].FileBaseName()+config.GrpcHeaderExtension,
				method.FileBaseName(),
				grpcServices,
				AsyncClientHeaderTemplate,
				method); err != nil {
				log.Fatal(err)
			}

			if err := generateGrpcFile(config.CppGenGrpcDirectory+serviceMethods[0].FileBaseName()+config.GrpcCppExtension,
				method.FileBaseName(),
				grpcServices,
				AsyncClientCppHandleTemplate,
				method); err != nil {
				log.Fatal(err)
			}
		}()
	}

}
