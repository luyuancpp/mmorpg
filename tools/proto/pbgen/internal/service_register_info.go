package internal

import (
	"bufio"
	"fmt"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"
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

// ReadProtoFileService reads service information from a protobuf descriptor file.
func ReadProtoFileService(fd os.DirEntry) error {

	// Check if the file is a proto file
	if !util.IsProtoFile(fd) {
		return fmt.Errorf("not a proto file: %s", fd.Name())
	}

	// Initialize the index for services in the file
	fileServiceIndex := uint32(0)

	// Construct the path to the descriptor file
	descFilePath := filepath.Join(config.PbDescDirectory, fd.Name()+config.ProtoDescExtension)

	// Read the descriptor file
	data, err := os.ReadFile(descFilePath)
	if err != nil {
		return fmt.Errorf("failed to read descriptor set file: %v", err)
	}

	// Unmarshal the descriptor set
	fdSet := &descriptorpb.FileDescriptorSet{}
	if err := proto.Unmarshal(data, fdSet); err != nil {
		return fmt.Errorf("failed to unmarshal descriptor set: %v", err)
	}

	// Iterate through each file in the descriptor set
	for _, file := range fdSet.File {
		for _, service := range file.Service {
			// Create an RPCServiceInfo object for each service
			rpcServiceInfo := RPCServiceInfo{
				FdSet:                  fdSet,
				FileServiceIndex:       fileServiceIndex,
				ServiceDescriptorProto: service,
			}

			// Iterate through each method in the service
			for index, method := range service.Method {
				// Create an RPCMethod object for each method
				rpcMethodInfo := RPCMethod{
					Id:                     math.MaxUint64,
					Index:                  uint64(index),
					FdSet:                  fdSet,
					FileServiceIndex:       fileServiceIndex,
					ServiceDescriptorProto: service,
					MethodDescriptorProto:  method,
				}

				// Append the method info to the service info
				rpcServiceInfo.MethodInfo = append(rpcServiceInfo.MethodInfo, &rpcMethodInfo)

				// Increment the global message ID counter
				atomic.AddUint64(&MaxMessageId, 1)
			}

			// Store the service info in the global RpcServiceMap
			RpcServiceMap.Store(service.GetName(), &rpcServiceInfo)

			result, ok := FileServiceMap.Load(file.GetName())
			if ok {
				serviceList := result.([]*RPCServiceInfo)
				serviceList = append(serviceList, &rpcServiceInfo)
				FileServiceMap.Store(file.GetName(), serviceList)
			} else {
				var serviceList []*RPCServiceInfo
				serviceList = append(serviceList, &rpcServiceInfo)
				FileServiceMap.Store(file.GetName(), serviceList)
			}
			fileServiceIndex++
		}
	}

	// Return nil on successful completion
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
				_ = ReadProtoFileService(fd)
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
		if len(v) > 0 && !v[0].CcGenericServices() {
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

	var serviceInfoIncludeBuilder strings.Builder

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
		serviceInfoIncludeBuilder.WriteString(methods[0].ServiceInfoIncludeName())
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
				method.Service(),
				method.Method(),
				method.CppRequest(),
				method.CppResponse(),
				handlerClassName,
			))
			if strings.Contains(method.Path(), config.ProtoDirectoryNames[config.ClientPlayerDirIndex]) {
				initFuncBuilder.WriteString("gClientToServerMessageId.emplace(" + rpcId + ");\n")
			}
		}
		initFuncBuilder.WriteString("\n")
	}

	includeBuilder.WriteString("\n")
	classHandlerBuilder.WriteString("\n")
	messageIdHandlerBuilder.WriteString("\n")
	initFuncBuilder.WriteString("}\n")
	serviceInfoIncludeBuilder.WriteString("\n")

	// Write to file
	data := includeBuilder.String() + serviceInfoIncludeBuilder.String() + classHandlerBuilder.String() +
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
		className := method1Info.Service() + "Impl"
		includeData += config.IncludeBegin + method1Info.FileNameNoEx() + config.HandlerHeaderExtension + "\"\n"

		classData += "class " + className + " : public " + method1Info.Service() + "{};\n"
		instanceData += config.Tab + "g_player_service.emplace(\"" + method1Info.Service() +
			"\", std::make_unique<" + method1Info.Service() + config.HandlerFileName + ">(std::make_unique< " +
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
		className := method1Info.Service() + "Impl"
		includeData += config.IncludeBegin + method1Info.FileNameNoEx() + config.RepliedHandlerHeaderExtension + "\"\n"

		classData += "class " + className + " : public " + method1Info.Service() + "{};\n"
		instanceData += config.Tab + "g_player_service_replied.emplace(\"" + method1Info.Service() +
			"\", std::make_unique<" + method1Info.Service() + config.RepliedHandlerFileName + ">(std::make_unique<" +
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
	go writePlayerServiceInstanceFiles("instance", IsGsPlayerHandler, config.GameNodePlayerMethodHandlerDirectory, config.PlayerServiceName)
	util.Wg.Add(1)
	go writePlayerServiceInstanceFiles("instance", isCentrePlayerHandler, config.CentreNodePlayerMethodHandlerDirectory, config.PlayerServiceName)
	util.Wg.Add(1)
	go writePlayerServiceInstanceFiles("repliedInstance", isGsPlayerRepliedHandler, config.GameNodePlayerMethodRepliedHandlerDirectory, config.PlayerRepliedServiceName)
	util.Wg.Add(1)
	go writePlayerServiceInstanceFiles("repliedInstance", isCentrePlayerRepliedHandler, config.CentrePlayerMethodRepliedHandlerDirectory, config.PlayerRepliedServiceName)
}
