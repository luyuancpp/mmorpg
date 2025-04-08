package gen

import (
	"bufio"
	"fmt"
	"log"
	"math"
	"os"
	"path/filepath"
	"pbgen/config"
	"pbgen/util"
	"sort"
	"strconv"
	"strings"
	"sync/atomic"
)

// ReadProtoFileService reads service information from a protobuf file.
func ReadProtoFileService(fd os.DirEntry, filePath string) error {

	if !util.IsProtoFile(fd) {
		return fmt.Errorf("not a proto file: %s", fd.Name())
	}

	f, err := os.Open(filePath + fd.Name())
	if err != nil {
		return fmt.Errorf("failed to open file %s: %w", fd.Name(), err)
	}
	defer f.Close()

	scanner := bufio.NewScanner(f)
	var service string
	var methodIndex uint64
	ccGenericServices := false
	var goPackageName string
	var pbPackageName string

	for scanner.Scan() {
		line := scanner.Text()

		if strings.Contains(line, config.CcGenericServices) {
			ccGenericServices = true
		}

		if strings.Contains(line, config.GoPackage) {
			goPackageName = strings.ReplaceAll(strings.Split(line, " ")[3], ";", "")
		} else if strings.Contains(line, config.PbPackage) {
			pbPackageName = strings.ReplaceAll(strings.Split(line, " ")[1], ";", "")
		}

		if strings.Contains(line, "service ") && !strings.Contains(line, "=") {
			service = strings.ReplaceAll(strings.Split(line, " ")[1], "{", "")
			rpcServiceInfo := RPCServiceInfo{}
			rpcServiceInfo.FileName = fd.Name()
			rpcServiceInfo.Path = filePath
			RpcServiceMap.Store(service, &rpcServiceInfo)
			continue
		} else if strings.Contains(line, "rpc ") {
			// 去除多余的空格
			line = rpcLineReplacer.Replace(strings.Trim(line, " "))
			// 分割 line 按空格分割
			splitList := strings.Split(line, " ")

			// 检查是否是流式 RPC
			var requestType, responseType string
			if strings.Contains(splitList[2], "stream") {
				requestType = splitList[3]
				responseType = splitList[6]

			} else {
				requestType = splitList[2]
				responseType = splitList[4]
			}

			// 创建 RPCMethod 实例
			rpcMethodInfo := RPCMethod{
				Service:           service,
				Method:            splitList[1],
				Request:           strings.Replace(requestType, ".", "::", -1),
				Response:          strings.Replace(responseType, ".", "::", -1),
				Id:                math.MaxUint64,
				Index:             methodIndex,
				FileName:          fd.Name(),
				Path:              filePath,
				CcGenericServices: ccGenericServices,
				PbPackage:         pbPackageName,
				GoPackage:         goPackageName,
			}

			result, ok := RpcServiceMap.Load(service)
			if !ok {
				fmt.Errorf("error reading file %s: %w", fd.Name(), err)
			}
			rpcServiceInfo := result.(*RPCServiceInfo)
			rpcServiceInfo.MethodInfo = append(rpcServiceInfo.MethodInfo, &rpcMethodInfo)
			atomic.AddUint64(&MaxMessageId, 1)
			methodIndex++
			continue
		}
	}

	if err := scanner.Err(); err != nil {
		return fmt.Errorf("error reading file %s: %w", fd.Name(), err)
	}

	return nil
}

// ReadProtoFileGrpcService reads gRPC service information from a protobuf file.
func ReadProtoFileGrpcService(fd os.DirEntry, filePath string) error {

	if !util.IsProtoFile(fd) {
		return fmt.Errorf("not a proto file: %s", fd.Name())
	}

	f, err := os.Open(filePath + fd.Name())
	if err != nil {
		return fmt.Errorf("failed to open file %s: %w", fd.Name(), err)
	}
	defer f.Close()

	scanner := bufio.NewScanner(f)
	ccGenericServices := false

	for scanner.Scan() {
		line := scanner.Text()

		if strings.Contains(line, config.CcGenericServices) {
			ccGenericServices = true
		}

		if ccGenericServices {
			continue
		}

		if strings.Contains(line, "service ") && !strings.Contains(line, "=") {
			GrpcServiceFileMap.Store(fd.Name(), "")
			break
		}
	}

	if err := scanner.Err(); err != nil {
		return fmt.Errorf("error reading file %s: %w", fd.Name(), err)
	}

	return nil
}

// ReadAllProtoFileServices reads all service information from protobuf files in configured directories.
func ReadAllProtoFileServices() {
	for i := 0; i < len(config.ProtoDirs); i++ {
		fds, _ := os.ReadDir(config.ProtoDirs[i])
		for _, v := range fds {
			util.Wg.Add(1)
			fd := v
			go func(i int, fd os.DirEntry) {
				defer util.Wg.Done()
				_ = ReadProtoFileService(fd, config.ProtoDirs[i])
			}(i, fd)

			util.Wg.Add(1)
			go func(i int, fd os.DirEntry) {
				defer util.Wg.Done()
				_ = ReadProtoFileGrpcService(fd, config.ProtoDirs[i])
			}(i, fd)
		}
	}
}

// ReadServiceIdFile reads service IDs from a file asynchronously.
func ReadServiceIdFile() {
	util.Wg.Add(1)
	go func() {
		defer util.Wg.Done()

		f, err := os.Open(config.ServiceIdFilePath)
		if err != nil {
			fmt.Errorf("failed to open file %s: %w", config.ServiceIdFilePath, err)
			log.Fatalf("error reading service ID file: %v", err)
		}
		defer f.Close()

		scanner := bufio.NewScanner(f)
		for scanner.Scan() {
			line := scanner.Text()
			splitList := strings.Split(line, "=")
			id, err := strconv.ParseUint(splitList[0], 10, 64)
			if err != nil {
				fmt.Errorf("failed to parse ID from line %s: %w", line, err)
				log.Fatalf("error reading service ID file: %v", err)
			}
			ServiceIdMap[splitList[1]] = id
		}

		if err := scanner.Err(); err != nil {
			fmt.Errorf("error reading file %s: %w", config.ServiceIdFilePath, err)
			log.Fatalf("error reading service ID file: %v", err)
		}

	}()
}

func WriteServiceIdFile() {
	util.Wg.Add(1)
	go func() {
		defer util.Wg.Done()

		var data string
		var idList []uint64
		for k, _ := range RpcIdMethodMap {
			idList = append(idList, k)
		}
		sort.Slice(idList, func(i, j int) bool { return idList[i] < idList[j] })
		for i := 0; i < len(idList); i++ {
			rpcMethodInfo, ok := RpcIdMethodMap[idList[i]]
			if !ok {
				fmt.Println("msg id=", strconv.Itoa(i), " not use ")
				continue
			}
			data += strconv.FormatUint(rpcMethodInfo.Id, 10) + "=" + (*rpcMethodInfo).KeyName() + "\n"
		}
		util.WriteMd5Data2File(config.ServiceIdFilePath, data)
	}()
}

// InitServiceId initializes service IDs based on the loaded service methods and ID mappings.
func InitServiceId() {
	var unUseServiceId = make(map[uint64]EmptyStruct)
	var useServiceId = make(map[uint64]EmptyStruct)

	RpcServiceMap.Range(func(k, v interface{}) bool {
		key := k.(string)
		methodInfo := v.(*RPCServiceInfo).MethodInfo
		ServiceMethodMap[key] = methodInfo
		return true
	})

	for _, methodList := range ServiceMethodMap {
		for _, mv := range methodList {
			id, ok := ServiceIdMap[mv.KeyName()]
			if !ok {
				//Id文件未找到则是新消息,或者已经改名，新消息后面处理，这里不处理
				continue
			}
			if MessageIdFileMaxId < id {
				MessageIdFileMaxId = id
			}
			useServiceId[id] = EmptyStruct{}
			mv.Id = id
		}
	}

	for i := uint64(0); i < MaxMessageId; i++ {
		if _, ok := useServiceId[i]; !ok {
			unUseServiceId[i] = EmptyStruct{}
		}
	}

	for _, methodList := range ServiceMethodMap {
		for _, mv := range methodList {
			if len(unUseServiceId) > 0 && mv.Id == math.MaxUint64 {
				for uk := range unUseServiceId {
					mv.Id = uk
					RpcIdMethodMap[mv.Id] = mv
					delete(unUseServiceId, uk)
					break
				}
				continue
			}
			if mv.Id == math.MaxUint64 {
				MessageIdFileMaxId++
				mv.Id = MessageIdFileMaxId
			}
			RpcIdMethodMap[mv.Id] = mv
			if FileMaxMessageId < mv.Id && mv.Id != math.MaxUint64 {
				FileMaxMessageId = mv.Id
			}
		}
	}
}

// GetSortServiceList returns a sorted list of service names.
func GetSortServiceList() []string {
	var ServiceList []string
	for k, v := range ServiceMethodMap {
		if len(v) > 0 && !v[0].CcGenericServices {
			continue
		}
		ServiceList = append(ServiceList, k)
	}
	sort.Strings(ServiceList)
	return ServiceList
}

// writeServiceInfoCppFile writes service information to a C++ file.
func writeServiceInfoCppFile() {
	defer util.Wg.Done()

	var includeBuilder strings.Builder
	includeBuilder.WriteString("#include <array>\n")
	includeBuilder.WriteString("#include \"service_info.h\"\n")

	var servcieInfoIncludeBuilder strings.Builder

	var classHandlerBuilder strings.Builder
	var initFuncBuilder strings.Builder
	var messageIdHandlerBuilder strings.Builder

	initFuncBuilder.WriteString("std::unordered_set<uint32_t> gClientToServerMessageId;\n")
	initFuncBuilder.WriteString("std::array<RpcService, " + strconv.FormatUint(MessageIdLen(), 10) + "> gMessageInfo;\n\n")
	initFuncBuilder.WriteString("void InitMessageInfo()\n{\n")

	// Collect all service list and generate include statements and class handlers
	serviceList := GetSortServiceList()
	for _, serviceName := range serviceList {
		methods := ServiceMethodMap[serviceName]
		if len(methods) == 0 {
			continue
		}

		includeBuilder.WriteString(methods[0].IncludeName())
		servcieInfoIncludeBuilder.WriteString(methods[0].ServiceInfoIncludeName())
		handlerClassName := serviceName + "Impl"
		classHandlerBuilder.WriteString("class " + handlerClassName + " final : public " + serviceName + "{};\n")
	}

	// Generate initialization functions
	for _, serviceName := range serviceList {
		methods := ServiceMethodMap[serviceName]
		for _, method := range methods {
			rpcId := method.KeyName() + config.MessageIdName
			handlerClassName := serviceName + "Impl"
			initFuncBuilder.WriteString(fmt.Sprintf(
				"gMessageInfo[%s] = RpcService{"+
					"\"%s\","+
					"\"%s\","+
					"\"%s\","+
					"\"%s\","+
					"std::make_unique_for_overwrite<%s>()};\n",
				rpcId,
				method.Service,
				method.Method,
				method.Request,
				method.Response,
				handlerClassName,
			))
			if strings.Contains(method.Path, config.ProtoDirectoryNames[config.ClientPlayerDirIndex]) {
				initFuncBuilder.WriteString("gClientToServerMessageId.emplace(" + rpcId + ");\n")
			}
		}
		initFuncBuilder.WriteString("\n")
	}

	includeBuilder.WriteString("\n")
	classHandlerBuilder.WriteString("\n")
	messageIdHandlerBuilder.WriteString("\n")
	initFuncBuilder.WriteString("}\n")
	servcieInfoIncludeBuilder.WriteString("\n")

	// Write to file
	data := includeBuilder.String() + servcieInfoIncludeBuilder.String() + classHandlerBuilder.String() +
		messageIdHandlerBuilder.String() + initFuncBuilder.String()
	util.WriteMd5Data2File(config.ServiceCppFilePath, data)
}

// writeServiceInfoHeadFile writes service information to a header file.
func writeServiceInfoHeadFile() {
	defer util.Wg.Done()
	var data strings.Builder

	data.WriteString("#pragma once\n")
	data.WriteString("#include <memory>\n")
	data.WriteString("#include <string>\n")
	data.WriteString("#include <array>\n")
	data.WriteString("#include <google/protobuf/message.h>\n")
	data.WriteString("#include <google/protobuf/service.h>\n\n")
	data.WriteString("struct RpcService\n{\n")
	data.WriteString(config.Tab + "const char* serviceName{nullptr};\n")
	data.WriteString(config.Tab + "const char* methodName{nullptr};\n")
	data.WriteString(config.Tab + "const char* request{nullptr};\n")
	data.WriteString(config.Tab + "const char* response{nullptr};\n")
	data.WriteString(config.Tab + "std::unique_ptr<::google::protobuf::Service> serviceImplInstance;\n};\n\n")
	data.WriteString("using MessageUniquePtr = std::unique_ptr<google::protobuf::Message>;\n\n")
	data.WriteString("void InitMessageInfo();\n\n")
	data.WriteString(fmt.Sprintf("constexpr uint32_t kMaxMessageLen = %d;\n\n", MessageIdLen()))
	data.WriteString(fmt.Sprintf("extern std::array<RpcService, kMaxMessageLen> gMessageInfo;\n\n"))
	data.WriteString("extern std::unordered_set<uint32_t> gClientToServerMessageId;\n")

	util.WriteMd5Data2File(config.ServiceHeaderFilePath, data.String())
}

// Helper function to generate instance data for player services.
func generateInstanceData(ServiceList []string, isPlayerHandlerFunc func(*RPCMethods) bool, handlerDir string, serviceName string) string {
	fileNameWithoutExt := serviceName[:len(serviceName)-len(filepath.Ext(serviceName))]

	var data strings.Builder
	includeData := "#include <memory>\n#include <unordered_map>\n#include \"" + fileNameWithoutExt + ".h\"\n\n"
	instanceData := ""
	classData := ""

	for _, key := range ServiceList {
		methodList, ok := ServiceMethodMap[key]
		if !ok || !isPlayerHandlerFunc(&methodList) {
			continue
		}

		method1Info := methodList[0]
		className := method1Info.Service + "Impl"
		includeData += config.IncludeBegin + method1Info.FileBaseName() + config.HandlerHeaderExtension + "\"\n"

		classData += "class " + className + " : public " + method1Info.Service + "{};\n"
		instanceData += config.Tab + "g_player_service.emplace(\"" + method1Info.Service +
			"\", std::make_unique<" + method1Info.Service + config.HandlerFileName + ">(std::make_unique< " +
			className + ">()));\n"
	}

	data.WriteString(includeData)
	data.WriteString("std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_service;\n\n")
	data.WriteString(classData)
	data.WriteString("void InitPlayerService()\n{\n")
	data.WriteString(instanceData)
	data.WriteString("}")

	util.WriteMd5Data2File(handlerDir+serviceName, data.String())

	return data.String()
}

// Helper function to generate instance data for player services.
func generateRepliedInstanceData(ServiceList []string, isPlayerHandlerFunc func(*RPCMethods) bool, handlerDir string, serviceName string) string {
	fileNameWithoutExt := serviceName[:len(serviceName)-len(filepath.Ext(serviceName))]

	var data strings.Builder
	includeData := "#include <memory>\n#include <unordered_map>\n#include \"" + fileNameWithoutExt + ".h\"\n\n"
	instanceData := ""
	classData := ""

	for _, key := range ServiceList {
		methodList, ok := ServiceMethodMap[key]
		if !ok || !isPlayerHandlerFunc(&methodList) {
			continue
		}

		method1Info := methodList[0]
		className := method1Info.Service + "Impl"
		includeData += config.IncludeBegin + method1Info.FileBaseName() + config.RepliedHandlerHeaderExtension + "\"\n"

		classData += "class " + className + " : public " + method1Info.Service + "{};\n"
		instanceData += config.Tab + "g_player_service_replied.emplace(\"" + method1Info.Service +
			"\", std::make_unique<" + method1Info.Service + config.RepliedHandlerFileName + ">(std::make_unique<" +
			className + ">()));\n"
	}

	data.WriteString(includeData)
	data.WriteString("std::unordered_map<std::string, std::unique_ptr<PlayerServiceReplied>> g_player_service_replied;\n\n")
	data.WriteString(classData)
	data.WriteString("void InitPlayerServiceReplied()\n{\n")
	data.WriteString(instanceData)
	data.WriteString("}")

	util.WriteMd5Data2File(handlerDir+serviceName, data.String())

	return data.String()
}

func writePlayerServiceInstanceFiles(serviceType string, isPlayerHandlerFunc func(*RPCMethods) bool, handlerDir, serviceName string) {
	defer util.Wg.Done()
	ServiceList := GetSortServiceList()
	var generatorFunc func([]string, func(*RPCMethods) bool, string, string) string

	if serviceType == "instance" {
		generatorFunc = generateInstanceData
	} else if serviceType == "repliedInstance" {
		generatorFunc = generateRepliedInstanceData
	} else {
		return // Handle error or unknown type
	}

	generatorFunc(ServiceList, isPlayerHandlerFunc, handlerDir, serviceName)
}

func WriteServiceRegisterInfoFile() {
	util.Wg.Add(1)
	go writeServiceInfoCppFile()
	util.Wg.Add(1)
	go writeServiceInfoHeadFile()
	util.Wg.Add(1)
	go writePlayerServiceInstanceFiles("instance", isGsPlayerHandler, config.GameNodePlayerMethodHandlerDirectory, config.PlayerServiceName)
	util.Wg.Add(1)
	go writePlayerServiceInstanceFiles("instance", isCentrePlayerHandler, config.CentreNodePlayerMethodHandlerDirectory, config.PlayerServiceName)
	util.Wg.Add(1)
	go writePlayerServiceInstanceFiles("repliedInstance", isGsPlayerRepliedHandler, config.GameNodePlayerMethodRepliedHandlerDirectory, config.PlayerRepliedServiceName)
	util.Wg.Add(1)
	go writePlayerServiceInstanceFiles("repliedInstance", isCentrePlayerRepliedHandler, config.CentrePlayerMethodRepliedHandlerDirectory, config.PlayerRepliedServiceName)
}
