package gen

import (
	"fmt"
	"log"
	"os"
	"path"
	"pbgen/config"
	"pbgen/util"
	"strings"
	"text/template"
)

type GrpcServiceTemplateData struct {
	ServiceInfo           []*RPCServiceInfo
	GrpcIncludeHeadName   string
	GeneratorGrpcFileName string
}

// 修改后的 generateGrpcFile 函数
func generateGrpcFile(fileName string, grpcServices []*RPCServiceInfo, text string) error {
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

	// 填充模板数据
	data := GrpcServiceTemplateData{
		ServiceInfo:           grpcServices,
		GrpcIncludeHeadName:   grpcServices[0].GrpcIncludeHeadName(),
		GeneratorGrpcFileName: grpcServices[0].GeneratorGrpcFileName(),
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

	FileServiceMap.Range(func(k, v interface{}) bool {
		protoFile := k.(string)
		serviceList := v.([]*RPCServiceInfo)
		util.Wg.Add(1)
		go func(protoFile string, serviceInfo []*RPCServiceInfo) {
			defer util.Wg.Done()

			if len(serviceInfo) <= 0 {
				return
			}

			firstService := serviceInfo[0]
			if !(strings.Contains(firstService.Path(), config.ProtoDirectoryNames[config.CommonProtoDirIndex]) ||
				strings.Contains(firstService.Path(), config.ProtoDirectoryNames[config.EtcdProtoDirIndex])) {
				return
			}

			os.MkdirAll(path.Dir(config.CppGenGrpcDirectory+protoFile), os.FileMode(0777))

			cppFileBaseName := firstService.ProtoPathWithFileBaseName()

			filePath := config.CppGenGrpcDirectory + cppFileBaseName + config.GrpcHeaderExtension
			if err := generateGrpcFile(filePath, serviceInfo, AsyncClientHeaderTemplate); err != nil {
				log.Fatal(err)
			}

			// 生成对应的 .cpp 文件
			filePathCpp := config.CppGenGrpcDirectory + cppFileBaseName + config.GrpcCppExtension
			if err := generateGrpcFile(filePathCpp, serviceInfo, AsyncClientCppHandleTemplate); err != nil {
				log.Fatal(err)
			}
		}(protoFile, serviceList)

		return true
	})
}
