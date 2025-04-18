package internal

import (
	"fmt"
	"log"
	"os"
	"path"
	"pbgen/config"
	"pbgen/util"
	"sort"
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

			// ✅ 排序逻辑
			sort.Slice(serviceInfo, func(i, j int) bool {
				return serviceInfo[i].FileServiceIndex < serviceInfo[j].FileServiceIndex
			})

			// 确保目录存在
			os.MkdirAll(path.Dir(config.CppGenGrpcDirectory+protoFile), os.FileMode(0777))

			cppFileBaseName := firstService.ProtoPathWithFileBaseName()

			// 生成 .h 文件
			filePath := config.CppGenGrpcDirectory + cppFileBaseName + config.GrpcHeaderExtension
			if err := generateGrpcFile(filePath, serviceInfo, AsyncClientHeaderTemplate); err != nil {
				log.Fatal(err)
			}

			// 生成 .cpp 文件
			filePathCpp := config.CppGenGrpcDirectory + cppFileBaseName + config.GrpcCppExtension
			if err := generateGrpcFile(filePathCpp, serviceInfo, AsyncClientCppHandleTemplate); err != nil {
				log.Fatal(err)
			}
		}(protoFile, serviceList)

		return true
	})
}
