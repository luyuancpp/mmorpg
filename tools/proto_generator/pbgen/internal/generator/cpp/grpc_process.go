package cpp

import (
	"bytes"
	"os"
	"path"
	"sort"
	"sync"
	"text/template"

	"go.uber.org/zap" // 引入zap用于结构化日志字段

	messageoption "github.com/luyuancpp/protooption"
	"pbgen/internal"
	_config "pbgen/internal/config"
	utils2 "pbgen/internal/utils"
	"pbgen/logger" // 引入全局logger包
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
	// 检查输入数据是否为空
	if len(grpcServices) == 0 {
		logger.Global.Fatal("生成gRPC文件失败: 服务信息不能为空",
			zap.String("target_file", fileName),
		)
	}

	// 渲染模板内容
	tmpl, err := template.New(fileName).Parse(text)
	if err != nil {
		logger.Global.Fatal("生成gRPC文件失败: 解析模板失败",
			zap.String("template_name", fileName),
			zap.Error(err),
		)
	}

	// 填充模板数据
	data := GrpcServiceTemplateData{
		ServiceInfo:           grpcServices,
		GrpcIncludeHeadName:   grpcServices[0].GrpcIncludeHeadName(),
		GeneratorGrpcFileName: grpcServices[0].GeneratorGrpcFileName(),
		Package:               grpcServices[0].Package(),
		GrpcCompleteQueueName: grpcServices[0].FileBaseNameCamel() + _config.Global.Naming.CompleteQueue,
		FileBaseNameCamel:     grpcServices[0].FileBaseNameCamel(),
	}

	var generatedContent bytes.Buffer
	// 渲染模板到缓冲区
	if err := tmpl.Execute(&generatedContent, data); err != nil {
		logger.Global.Fatal("生成gRPC文件失败: 执行模板失败",
			zap.String("target_file", fileName),
			zap.Error(err),
		)
	}

	// 读取现有文件的内容（如果文件存在）
	existingContent, err := os.ReadFile(fileName)
	if err != nil && !os.IsNotExist(err) {
		logger.Global.Fatal("生成gRPC文件失败: 读取现有文件失败",
			zap.String("file_path", fileName),
			zap.Error(err),
		)
	}

	// 如果文件内容相同，则跳过写入
	if err == nil && bytes.Equal(existingContent, generatedContent.Bytes()) {
		logger.Global.Info("gRPC文件内容无变化，跳过写入",
			zap.String("file_path", fileName),
		)
		return nil
	}

	err = os.MkdirAll(path.Dir(fileName), os.FileMode(0777))
	if err != nil {
		logger.Global.Error("生成gRPC文件失败: 创建目录失败",
			zap.String("directory", path.Dir(fileName)),
			zap.Error(err),
		)
		return err
	}

	// 创建文件并写入渲染后的内容
	file, err := os.Create(fileName)
	if err != nil {
		logger.Global.Fatal("生成gRPC文件失败: 创建文件失败",
			zap.String("file_path", fileName),
			zap.Error(err),
		)
	}
	defer file.Close()

	// 写入生成的内容到文件
	if _, err := file.Write(generatedContent.Bytes()); err != nil {
		logger.Global.Fatal("生成gRPC文件失败: 写入文件失败",
			zap.String("file_path", fileName),
			zap.Error(err),
		)
	}

	logger.Global.Info("gRPC文件已更新",
		zap.String("file_path", fileName),
	)
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
			protoPath := firstService.Path()

			// 如果既不是gRPC服务也不是etcd服务，则返回（不继续处理）
			if !(utils2.HasGrpcService(protoPath) || utils2.HasEtcdService(protoPath)) {
				return
			}

			// 以下为继续处理的逻辑
			// ...
			sort.Slice(serviceInfo, func(i, j int) bool {
				return serviceInfo[i].ServiceIndex < serviceInfo[j].ServiceIndex
			})

			// 确保目录存在
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

			// 生成 .h 文件
			filePath := _config.Global.Paths.CppGenGrpcDir + cppFileBaseName + _config.Global.FileExtensions.GrpcClientH
			if err := generateGrpcFile(filePath, serviceInfo, AsyncClientHeaderTemplate); err != nil {
				logger.Global.Fatal("生成gRPC客户端头文件失败",
					zap.String("file_path", filePath),
					zap.String("proto_file", protoFile),
					zap.Error(err),
				)
			}

			// 生成 .cpp 文件
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
