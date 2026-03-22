package cpp

import (
	"bytes"
	"os"
	"path"
	"sort"
	"sync"
	"text/template"

	"go.uber.org/zap"

	"protogen/internal"
	_config "protogen/internal/config"
	utils2 "protogen/internal/utils"
	"protogen/logger"

	messageoption "github.com/luyuancpp/protooption"
)

// GrpcServiceTemplateData 用于传递给模板的数据结构
type GrpcServiceTemplateData struct {
	ServiceInfo           []*internal.RPCServiceInfo
	GrpcIncludeHeadName   string
	GeneratorGrpcFileName string
	Package               string
	GrpcCompleteQueueName string
	FileBaseNameCamel     string
}

// generateGrpcFile 根据模板生成 gRPC 文件，并避免重复写入
func generateGrpcFile(fileName string, grpcServices []*internal.RPCServiceInfo, text string) error {
	if len(grpcServices) == 0 {
		logger.Global.Fatal("生成gRPC文件失败: 服务信息不能为空",
			zap.String("target_file", fileName),
		)
	}

	tmpl, err := template.New(fileName).Parse(text)
	if err != nil {
		logger.Global.Fatal("生成gRPC文件失败: 解析模板失败",
			zap.String("template_name", fileName),
			zap.Error(err),
		)
	}

	data := GrpcServiceTemplateData{
		ServiceInfo:           grpcServices,
		GrpcIncludeHeadName:   grpcServices[0].GrpcIncludeHeadName(),
		GeneratorGrpcFileName: grpcServices[0].GeneratorGrpcFileName(),
		Package:               grpcServices[0].Package(),
		GrpcCompleteQueueName: grpcServices[0].FileBaseNameCamel() + _config.Global.Naming.CompleteQueue,
		FileBaseNameCamel:     grpcServices[0].FileBaseNameCamel(),
	}

	var generatedContent bytes.Buffer
	if err := tmpl.Execute(&generatedContent, data); err != nil {
		logger.Global.Fatal("生成gRPC文件失败: 执行模板失败",
			zap.String("target_file", fileName),
			zap.Error(err),
		)
	}

	if err := utils2.WriteFileIfChanged(fileName, []byte(utils2.NormalizeGeneratedLayout(generatedContent.String()))); err != nil {
		logger.Global.Error("生成gRPC文件失败: 写入文件失败",
			zap.String("file_path", fileName),
			zap.String("directory", path.Dir(fileName)),
			zap.Error(err),
		)
		return err
	}

	return nil
}

func CppGrpcCallClient(wg *sync.WaitGroup) {
	internal.FileServiceMap.Range(func(k, v interface{}) bool {
		protoFile := k.(string)
		serviceList := v.([]*internal.RPCServiceInfo)
		wg.Add(1)
		go func(protoFile string, serviceInfo []*internal.RPCServiceInfo) {
			defer wg.Done()

			if len(serviceInfo) <= 0 {
				return
			}

			firstService := serviceInfo[0]
			_ = firstService.Path()

			// Generate per-file async grpc clients for proto files that use gRPC services.
			// CcGenericServices=true means legacy C++ generic RPC handlers, not gRPC stubs.
			if firstService.CcGenericServices() {
				return
			}

			// ...
			sort.Slice(serviceInfo, func(i, j int) bool {
				return serviceInfo[i].ServiceIndex < serviceInfo[j].ServiceIndex
			})

			targetDir := path.Dir(_config.Global.Paths.CppGenGrpcDir + firstService.LogicalPath())
			err := os.MkdirAll(targetDir, os.FileMode(0777))
			if err != nil {
				logger.Global.Fatal("生成gRPC客户端代码失败: 创建目录失败",
					zap.String("directory", targetDir),
					zap.String("proto_file", protoFile),
					zap.Error(err),
				)
				return
			}

			cppFileBaseName := firstService.LogicalPath()

			filePath := _config.Global.Paths.CppGenGrpcDir + cppFileBaseName + _config.Global.FileExtensions.GrpcClientH
			if err := generateGrpcFile(filePath, serviceInfo, AsyncClientHeaderTemplate); err != nil {
				logger.Global.Fatal("生成gRPC客户端头文件失败",
					zap.String("file_path", filePath),
					zap.String("proto_file", protoFile),
					zap.Error(err),
				)
			}

			filePathCpp := _config.Global.Paths.CppGenGrpcDir + cppFileBaseName + _config.Global.FileExtensions.GrpcClientCpp
			if err := generateGrpcFile(filePathCpp, serviceInfo, AsyncClientCppHandleTemplate); err != nil {
				logger.Global.Fatal("生成gRPC客户端cpp文件失败",
					zap.String("file_path", filePathCpp),
					zap.String("proto_file", protoFile),
					zap.Error(err),
				)
			}
		}(protoFile, serviceList)

		return true
	})

	{
		wg.Add(1)

		go func() {
			defer wg.Done()
			m := map[string]*internal.RPCServiceInfo{}
			serviceInfoList := make([]*internal.RPCServiceInfo, 0)
			for _, service := range internal.GlobalRPCServiceList {
				if service.CcGenericServices() {
					continue
				}

				if internal.IsFileBelongToNode(service.Fd, messageoption.NodeType_NODE_DB) {
					continue
				}

				if _, ok := m[service.FileBaseNameCamel()]; ok {
					continue
				}
				m[service.FileBaseNameCamel()] = service
				serviceInfoList = append(serviceInfoList, service)
			}

			// Stabilize namespace/branch emission order in grpc_init_total templates.
			sort.Slice(serviceInfoList, func(i, j int) bool {
				left := serviceInfoList[i]
				right := serviceInfoList[j]

				if left.BasePathForCpp() != right.BasePathForCpp() {
					return left.BasePathForCpp() < right.BasePathForCpp()
				}
				if left.Package() != right.Package() {
					return left.Package() < right.Package()
				}
				return left.FileBaseNameCamel() < right.FileBaseNameCamel()
			})

			err := os.MkdirAll(path.Dir(_config.Global.Paths.GrpcInitCppFile), os.FileMode(0777))
			if err != nil {
				logger.Global.Error("生成gRPC初始化文件失败: 创建目录失败",
					zap.String("directory", path.Dir(_config.Global.Paths.GrpcInitCppFile)),
					zap.Error(err),
				)
				return
			}

			cppData := struct {
				ServiceInfo []*internal.RPCServiceInfo
			}{
				ServiceInfo: serviceInfoList,
			}

			if err := utils2.RenderTemplateToFile("internal/template/grpc_init_total.cpp.tmpl", _config.Global.Paths.GrpcInitCppFile, cppData); err != nil {
				logger.Global.Fatal("生成gRPC初始化cpp文件失败",
					zap.String("file_path", _config.Global.Paths.GrpcInitCppFile),
					zap.Error(err),
				)
			}

			if err := utils2.RenderTemplateToFile("internal/template/grpc_init_total.h.tmpl", _config.Global.Paths.GrpcInitHeadFile, cppData); err != nil {
				logger.Global.Fatal("生成gRPC初始化头文件失败",
					zap.String("file_path", _config.Global.Paths.GrpcInitHeadFile),
					zap.Error(err),
				)
			}
		}()
	}
}
