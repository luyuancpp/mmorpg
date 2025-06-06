package internal

import (
	"bytes"
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

// GrpcServiceTemplateData 用于传递给模板的数据结构
type GrpcServiceTemplateData struct {
	ServiceInfo           []*RPCServiceInfo
	GrpcIncludeHeadName   string
	GeneratorGrpcFileName string
	Package               string
}

// generateGrpcFile 根据模板生成 gRPC 文件，并避免重复写入
func generateGrpcFile(fileName string, grpcServices []*RPCServiceInfo, text string) error {
	// 检查输入数据是否为空
	if len(grpcServices) == 0 {
		return fmt.Errorf("grpcServices cannot be empty")
	}

	// 渲染模板内容
	tmpl, err := template.New(fileName).Parse(text)
	if err != nil {
		return fmt.Errorf("could not parse template: %w", err)
	}

	// 填充模板数据
	data := GrpcServiceTemplateData{
		ServiceInfo:           grpcServices,
		GrpcIncludeHeadName:   grpcServices[0].GrpcIncludeHeadName(),
		GeneratorGrpcFileName: grpcServices[0].GeneratorGrpcFileName(),
		Package:               grpcServices[0].Package(),
	}

	var generatedContent bytes.Buffer
	// 渲染模板到缓冲区
	if err := tmpl.Execute(&generatedContent, data); err != nil {
		return fmt.Errorf("could not execute template: %w", err)
	}

	// 读取现有文件的内容（如果文件存在）
	existingContent, err := os.ReadFile(fileName)
	if err != nil && !os.IsNotExist(err) {
		return fmt.Errorf("could not read existing file: %w", err)
	}

	// 如果文件内容相同，则跳过写入
	if err == nil && bytes.Equal(existingContent, generatedContent.Bytes()) {
		fmt.Println("文件内容没有变化，跳过写入:", fileName)
		return nil
	}

	// 创建文件并写入渲染后的内容
	file, err := os.Create(fileName)
	if err != nil {
		return fmt.Errorf("could not create file: %w", err)
	}
	defer file.Close()

	// 写入生成的内容到文件
	if _, err := file.Write(generatedContent.Bytes()); err != nil {
		return fmt.Errorf("could not write to file: %w", err)
	}

	fmt.Println("文件已更新:", fileName)
	return nil
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
			if !(strings.Contains(firstService.Path(), config.ProtoDirectoryNames[config.EtcdProtoDirIndex]) ||
				strings.Contains(firstService.Path(), config.ProtoDirectoryNames[config.LoginProtoDirIndex])) {
				return
			}

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

	{
		util.Wg.Add(1)

		go func() {
			defer util.Wg.Done()
			m := map[string]*RPCServiceInfo{}
			serviceInfoList := make([]*RPCServiceInfo, 0)
			serviceList := GetSortServiceList()
			for _, serviceName := range serviceList {
				result, ok := RpcServiceMap.Load(serviceName)
				if !ok {
					continue
				}
				service := result.(*RPCServiceInfo)
				if service.CcGenericServices() {
					continue
				}
				if util.IsPathInProtoDirs(service.Path(), config.DbProtoDirIndex) {
					continue
				}
				if _, ok := m[service.FileBaseNameCamel()]; ok {
					continue
				}
				m[service.FileBaseNameCamel()] = service
				serviceInfoList = append(serviceInfoList, service)
			}

			// 确保目录存在
			os.MkdirAll(path.Dir(config.GrpcInitFileCppPath), os.FileMode(0777))

			// Prepare the data for C++ source file
			cppData := struct {
				ServiceInfo []*RPCServiceInfo
			}{
				ServiceInfo: serviceInfoList,
			}
			// 生成 .h 文件
			if err := RenderTemplateToFile("internal/gen/template/grpc_init_total.cpp.tmpl", config.GrpcInitFileCppPath, cppData); err != nil {
				log.Fatal(err)
			}

			if err := RenderTemplateToFile("internal/gen/template/grpc_init_total.h.tmpl", config.GrpcInitFileHeadPath, cppData); err != nil {
				log.Fatal(err)
			}
		}()
	}

}
