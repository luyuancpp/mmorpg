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

type GrpcServiceTemplateData struct {
	GrpcMethod                    []RPCMethod
	Service                       string
	GeneratorFileName             string
	GrpcIncludeHeadName           string
	GetServiceFullNameWithNoColon string
}

// 修改后的 generateGrpcFile 函数
func generateGrpcFile(fileName string, grpcServices []RPCMethod, text string) error {
	// 创建文件
	file, err := os.Create(fileName)
	if err != nil {
		return fmt.Errorf("could not create file: %w", err)
	}
	defer file.Close()

	// 模板内容
	tmpl, err := template.New(fileName).Parse(text)
	if err != nil {
		return fmt.Errorf("could not parse template: %w", err)
	}

	firstService := grpcServices[0]

	// 填充模板数据
	data := GrpcServiceTemplateData{
		GrpcMethod:                    grpcServices,
		GrpcIncludeHeadName:           firstService.GrpcIncludeHeadName(),
		GeneratorFileName:             filepath.Base(strings.TrimSuffix(fileName, filepath.Ext(fileName))),
		GetServiceFullNameWithNoColon: firstService.GetServiceFullNameWithNoColon(),
	}

	// 将内容写入文件
	return tmpl.Execute(file, data)
}

func CppGrpcCallClient() {
	// 以 proto 文件为单位处理服务
	protoServiceMap := make(map[string][]*RPCMethod) // 将服务按 proto 文件分组

	for _, serviceMethods := range ServiceMethodMap {
		// 对每个 serviceMethods，按 Proto 文件进行分组
		protoFile := serviceMethods[0].FileNameNoEx() // 假设所有服务在同一个 proto 文件
		protoServiceMap[protoFile] = append(protoServiceMap[protoFile], serviceMethods...)
	}

	// 对每个 proto 文件生成一个文件
	for protoFile, serviceMethods := range protoServiceMap {
		util.Wg.Add(1)
		go func(protoFile string, serviceMethods []*RPCMethod) {
			defer util.Wg.Done()

			if len(serviceMethods) <= 0 {
				return
			}

			if !(strings.Contains(serviceMethods[0].Path(), config.ProtoDirectoryNames[config.CommonProtoDirIndex]) ||
				strings.Contains(serviceMethods[0].Path(), config.ProtoDirectoryNames[config.EtcdProtoDirIndex])) {
				return
			}

			grpcServices := make([]RPCMethod, 0)

			for _, method := range serviceMethods {
				grpcServices = append(grpcServices, *method)
			}

			filePath := config.CppGenGrpcDirectory + protoFile + config.GrpcHeaderExtension
			if err := generateGrpcFile(filePath, grpcServices, AsyncClientHeaderTemplate); err != nil {
				log.Fatal(err)
			}

			// 生成对应的 .cpp 文件
			filePathCpp := config.CppGenGrpcDirectory + protoFile + config.GrpcCppExtension
			if err := generateGrpcFile(filePathCpp, grpcServices, AsyncClientCppHandleTemplate); err != nil {
				log.Fatal(err)
			}
		}(protoFile, serviceMethods)
	}
}
