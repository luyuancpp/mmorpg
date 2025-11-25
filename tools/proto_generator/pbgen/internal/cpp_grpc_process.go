package internal

import (
	"bytes"
	"fmt"
	messageoption "github.com/luyuancpp/protooption"
	"log"
	"os"
	"path"
	"pbgen/config"
	"pbgen/utils"
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
	GrpcCompleteQueueName string
	FileBaseNameCamel     string
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
		GrpcCompleteQueueName: grpcServices[0].FileBaseNameCamel() + config.CompleteQueueName,
		FileBaseNameCamel:     grpcServices[0].FileBaseNameCamel(),
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

	err = os.MkdirAll(path.Dir(fileName), os.FileMode(0777))
	if err != nil {
		return err
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
		utils.Wg.Add(1)
		go func(protoFile string, serviceInfo []*RPCServiceInfo) {
			defer utils.Wg.Done()

			if len(serviceInfo) <= 0 {
				return
			}

			firstService := serviceInfo[0]
			protoPath := firstService.Path()

			// 如果既不是gRPC服务也不是etcd服务，则返回（不继续处理）
			if !utils.HasGrpcService(strings.ToLower(protoPath)) && !utils.HasEtcdService(strings.ToLower(protoPath)) {
				return
			}

			// 以下为继续处理的逻辑
			// ...
			sort.Slice(serviceInfo, func(i, j int) bool {
				return serviceInfo[i].ServiceIndex < serviceInfo[j].ServiceIndex
			})

			// 确保目录存在
			err := os.MkdirAll(path.Dir(config.CppGenGrpcDirectory+firstService.LogicalPath()), os.FileMode(0777))
			if err != nil {
				log.Fatal(err)
				return
			}

			cppFileBaseName := firstService.LogicalPath()

			// 生成 .h 文件
			filePath := config.CppGenGrpcDirectory + cppFileBaseName + config.GrpcClientHeaderExtension
			if err := generateGrpcFile(filePath, serviceInfo, AsyncClientHeaderTemplate); err != nil {
				log.Fatal(err)
			}

			// 生成 .cpp 文件
			filePathCpp := config.CppGenGrpcDirectory + cppFileBaseName + config.GrpcClientCppExtension
			if err := generateGrpcFile(filePathCpp, serviceInfo, AsyncClientCppHandleTemplate); err != nil {
				log.Fatal(err)
			}
		}(protoFile, serviceList)

		return true
	})

	{
		utils.Wg.Add(1)

		go func() {
			defer utils.Wg.Done()
			m := map[string]*RPCServiceInfo{}
			serviceInfoList := make([]*RPCServiceInfo, 0)
			for _, service := range GlobalRPCServiceList {
				if service.CcGenericServices() {
					continue
				}

				if IsFileBelongToNode(service.Fd, messageoption.NodeType_NODE_DB) {
					continue
				}

				if _, ok := m[service.FileBaseNameCamel()]; ok {
					continue
				}
				m[service.FileBaseNameCamel()] = service
				serviceInfoList = append(serviceInfoList, service)
			}

			os.MkdirAll(path.Dir(config.GrpcInitFileCppPath), os.FileMode(0777))

			cppData := struct {
				ServiceInfo []*RPCServiceInfo
			}{
				ServiceInfo: serviceInfoList,
			}

			if err := utils.RenderTemplateToFile("internal/template/grpc_init_total.cpp.tmpl", config.GrpcInitFileCppPath, cppData); err != nil {
				log.Fatal(err)
			}

			if err := utils.RenderTemplateToFile("internal/template/grpc_init_total.h.tmpl", config.GrpcInitFileHeadPath, cppData); err != nil {
				log.Fatal(err)
			}
		}()
	}

}
