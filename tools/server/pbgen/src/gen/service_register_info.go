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
	defer util.Wg.Done()

	if !util.IsProtoFile(fd) {
		return fmt.Errorf("not a proto file: %s", fd.Name())
	}

	f, err := os.Open(filepath.Join(filePath, fd.Name()))
	if err != nil {
		return fmt.Errorf("failed to open file %s: %w", fd.Name(), err)
	}
	defer f.Close()

	scanner := bufio.NewScanner(f)
	var service string
	var methodIndex uint64
	var rpcServiceInfo RPCServiceInfo
	ccGenericServices := false

	for scanner.Scan() {
		line := scanner.Text()

		if strings.Contains(line, config.CcGenericServices) {
			ccGenericServices = true
		}

		if strings.Contains(line, "service ") && !strings.Contains(line, "=") {
			service = strings.ReplaceAll(strings.Split(line, " ")[1], "{", "")
			rpcServiceInfo.FileName = fd.Name()
			rpcServiceInfo.Path = filePath
			RpcServiceMap.Store(service, &rpcServiceInfo)
			continue
		} else if strings.Contains(line, "rpc ") {
			line = rpcLineReplacer.Replace(strings.Trim(line, " "))
			splitList := strings.Split(line, " ")
			rpcMethodInfo := RPCMethod{
				Service:           service,
				Method:            splitList[1],
				Request:           strings.Replace(splitList[2], ".", "::", -1),
				Response:          strings.Replace(splitList[4], ".", "::", -1),
				Id:                math.MaxUint64,
				Index:             methodIndex,
				FileName:          fd.Name(),
				Path:              filePath,
				CcGenericServices: ccGenericServices,
			}
			rpcServiceInfo.MethodInfo = append(rpcServiceInfo.MethodInfo, &rpcMethodInfo)
			atomic.AddUint64(&MaxMessageId, 1)
			methodIndex++
			continue
		} else if len(service) > 0 && strings.Contains(line, "}") {
			break
		}
	}

	if err := scanner.Err(); err != nil {
		return fmt.Errorf("error reading file %s: %w", fd.Name(), err)
	}

	return nil
}

// ReadProtoFileGrpcService reads gRPC service information from a protobuf file.
func ReadProtoFileGrpcService(fd os.DirEntry, filePath string) error {
	defer util.Wg.Done()

	if !util.IsProtoFile(fd) {
		return fmt.Errorf("not a proto file: %s", fd.Name())
	}

	f, err := os.Open(filepath.Join(filePath, fd.Name()))
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
				_ = ReadProtoFileService(fd, config.ProtoDirs[i])
			}(i, fd)

			util.Wg.Add(1)
			go func(i int, fd os.DirEntry) {
				_ = ReadProtoFileGrpcService(fd, config.ProtoDirs[i])
			}(i, fd)
		}
	}
}

// readServiceIdFile reads service IDs from a file and populates the ServiceIdMap.
func readServiceIdFile() error {
	f, err := os.Open(config.ServiceIdsFileName)
	if err != nil {
		return fmt.Errorf("failed to open file %s: %w", config.ServiceIdsFileName, err)
	}
	defer f.Close()

	scanner := bufio.NewScanner(f)
	for scanner.Scan() {
		line := scanner.Text()
		splitList := strings.Split(line, "=")
		id, err := strconv.ParseUint(splitList[0], 10, 64)
		if err != nil {
			return fmt.Errorf("failed to parse ID from line %s: %w", line, err)
		}
		ServiceIdMap[splitList[1]] = id
	}

	if err := scanner.Err(); err != nil {
		return fmt.Errorf("error reading file %s: %w", config.ServiceIdsFileName, err)
	}

	return nil
}

// ReadServiceIdFile reads service IDs from a file asynchronously.
func ReadServiceIdFile() {
	util.Wg.Add(1)
	go func() {
		defer util.Wg.Done()
		if err := readServiceIdFile(); err != nil {
			log.Fatalf("error reading service ID file: %v", err)
		}
	}()
}

func writeServiceIdFile() {
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
	util.WriteMd5Data2File(config.ServiceIdsFileName, data)
}

func WriteServiceIdFile() {
	util.Wg.Add(1)
	go writeServiceIdFile()
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
	var data strings.Builder
	var idList []uint64

	for k := range RpcIdMethodMap {
		idList = append(idList, k)
	}
	sort.Slice(idList, func(i, j int) bool { return idList[i] < idList[j] })

	for i, id := range idList {
		rpcMethodInfo := RpcIdMethodMap[id]
		if rpcMethodInfo == nil {
			fmt.Println("msg id=", strconv.Itoa(i), " not used")
			continue
		}
		data.WriteString(fmt.Sprintf("%d=%s\n", rpcMethodInfo.Id, rpcMethodInfo.KeyName()))
	}

	util.WriteMd5Data2File(config.ServiceIdsFileName, data.String())
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
	data.WriteString(config.Tab + "const char* service{nullptr};\n")
	data.WriteString(config.Tab + "const char* method{nullptr};\n")
	data.WriteString(config.Tab + "const char* request{nullptr};\n")
	data.WriteString(config.Tab + "const char* response{nullptr};\n")
	data.WriteString(config.Tab + "std::unique_ptr<::google::protobuf::Service> service_impl_instance_;\n};\n\n")
	data.WriteString("using MessageUniquePtr = std::unique_ptr<google::protobuf::Message>;\n\n")
	data.WriteString("void InitMessageInfo();\n\n")
	data.WriteString(fmt.Sprintf("constexpr uint32_t kMaxMessageLen = %d;\n\n", MessageIdLen()))
	data.WriteString(fmt.Sprintf("extern std::array<RpcService, kMaxMessageLen> g_message_info;\n\n"))
	data.WriteString("extern std::unordered_set<uint32_t> g_c2s_service_id;\n")

	util.WriteMd5Data2File(config.ServiceHeadFileName, data.String())
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
		includeData += config.IncludeBegin + method1Info.FileBaseName() + config.HeadHandlerEx + "\"\n"

		classData += "class " + className + " : public " + method1Info.Service + "{};\n"
		instanceData += config.Tab + "g_player_service.emplace(\"" + method1Info.Service +
			"\", std::make_unique<" + method1Info.Service + config.HandlerName + ">(new " +
			className + "));\n"
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
		includeData += config.IncludeBegin + method1Info.FileBaseName() + config.HeadRepliedHandlerEx + "\"\n"

		classData += "class " + className + " : public " + method1Info.Service + "{};\n"
		instanceData += config.Tab + "g_player_service_replied.emplace(\"" + method1Info.Service +
			"\", std::make_unique<" + method1Info.Service + config.RepliedHandlerName + ">(new " +
			className + "));\n"
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

// Function to write gRPC global player service instance information.
func writeGsGlobalPlayerServiceInstanceFile() {
	defer util.Wg.Done()
	ServiceList := GetSortServiceList()
	generateInstanceData(ServiceList, isGsPlayerHandler, config.GsMethodHandleDir, config.PlayerServiceName)
}

// Function to write Centre global player service instance information.
func writeCentreGlobalPlayerServiceInstanceFile() {
	defer util.Wg.Done()
	ServiceList := GetSortServiceList()
	generateInstanceData(ServiceList, isCentrePlayerHandler, config.CentreMethodHandleDir, config.PlayerServiceName)
}

// Function to write gRPC global player service replied instance information.
func writeGsGlobalPlayerServiceRepliedInstanceFile() {
	defer util.Wg.Done()
	ServiceList := GetSortServiceList()
	generateRepliedInstanceData(ServiceList, isGsPlayerRepliedHandler, config.GsMethodRepliedHandleDir, config.PlayerRepliedServiceName)
}

// Function to write Centre global player service replied instance information.
func writeCentreGlobalPlayerServiceRepliedInstanceFile() {
	defer util.Wg.Done()
	ServiceList := GetSortServiceList()
	generateRepliedInstanceData(ServiceList, isCentrePlayerRepliedHandler, config.CentreMethodRepliedHandleDir, config.PlayerRepliedServiceName)
}

func WriteServiceRegisterInfoFile() {
	util.Wg.Add(1)
	go writeServiceInfoCppFile()
	util.Wg.Add(1)
	go writeServiceInfoHeadFile()
	util.Wg.Add(1)
	writeGsGlobalPlayerServiceInstanceFile()
	util.Wg.Add(1)
	writeCentreGlobalPlayerServiceInstanceFile()
	util.Wg.Add(1)
	writeGsGlobalPlayerServiceRepliedInstanceFile()
	util.Wg.Add(1)
	writeCentreGlobalPlayerServiceRepliedInstanceFile()
}
