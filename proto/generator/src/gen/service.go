package gen

import (
	"bufio"
	"fmt"
	"gengo/config"
	"gengo/util"
	"log"
	"math"
	"os"
	"sort"
	"strconv"
	"strings"
	"sync/atomic"
)

func ReadProtoFileService(fd os.DirEntry, filePath string) (err error) {
	defer util.Wg.Done()
	f, err := os.Open(filePath + fd.Name())
	if err != nil {
		log.Fatal(err)
		return err
	}
	defer func(f *os.File) {
		err := f.Close()
		if err != nil {
			log.Fatal(err)
		}
	}(f)

	scanner := bufio.NewScanner(f)
	var line string
	var service string
	var methodIndex uint64
	var rpcServiceInfo RpcServiceInfo
	ccGenericServices := false
	for scanner.Scan() {
		line = scanner.Text()
		if strings.Contains(line, config.CcGenericServices) {
			ccGenericServices = true
		}
		if !ccGenericServices {
			continue
		}
		if strings.Contains(line, "service ") && !strings.Contains(line, "=") {
			service = strings.ReplaceAll(line, "{", "")
			service = strings.Split(service, " ")[1]
			rpcServiceInfo.FileName = fd.Name()
			rpcServiceInfo.Path = filePath
			RpcServiceSyncMap.Store(service, &rpcServiceInfo)
			continue
		} else if strings.Contains(line, "rpc ") {
			line = rpcLineReplacer.Replace(strings.Trim(line, " "))
			splitList := strings.Split(line, " ")
			var rpcMethodInfo RpcMethodInfo
			rpcMethodInfo.Service = service
			rpcMethodInfo.Method = splitList[1]
			rpcMethodInfo.Request = strings.Replace(splitList[2], ".", "::", -1)
			rpcMethodInfo.Response = strings.Replace(splitList[4], ".", "::", -1)
			rpcMethodInfo.Id = math.MaxUint64
			rpcMethodInfo.Index = methodIndex
			rpcMethodInfo.FileName = fd.Name()
			rpcMethodInfo.Path = filePath
			rpcServiceInfo.MethodInfo = append(rpcServiceInfo.MethodInfo, &rpcMethodInfo)
			MaxMessageId = atomic.AddUint64(&MaxMessageId, 1)
			methodIndex += 1
			continue
		} else if len(service) > 0 && strings.Contains(line, "}") {
			break
		}

	}

	if err := scanner.Err(); err != nil {
		log.Fatal(err)
	}
	return err
}

func ReadAllProtoFileServices() {
	for i := 0; i < len(config.ProtoDirs); i++ {
		fds, err := os.ReadDir(config.ProtoDirs[i])
		if err != nil {
			log.Fatal(err)
			continue
		}
		for _, fd := range fds {
			if !util.IsProtoFile(fd) {
				continue
			}
			util.Wg.Add(1)
			ReadProtoFileService(fd, config.ProtoDirs[i])
		}
	}
}

func readServiceIdFile() {
	defer util.Wg.Done()
	f, err := os.Open(config.ServiceIdsFileName)
	if err != nil {
		log.Fatal(err)
		return
	}
	defer f.Close()

	scanner := bufio.NewScanner(f)
	var line string
	for scanner.Scan() {
		line = scanner.Text()
		splitList := strings.Split(line, "=")
		id, _ := strconv.ParseUint(splitList[0], 10, 64)
		ServiceIdMap[splitList[1]] = id

		if err != nil {
			log.Fatal(err)
		}
	}
}

func ReadServiceIdFile() {
	util.Wg.Add(1)
	go readServiceIdFile()
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
	os.WriteFile(config.ServiceIdsFileName, []byte(data), 0666)
}

func WriteServiceIdFile() {
	util.Wg.Add(1)
	go writeServiceIdFile()
}

func InitServiceId() {
	var unUseServiceId = map[uint64]EmptyStruct{}
	var useServiceId = map[uint64]EmptyStruct{}

	RpcServiceSyncMap.Range(func(k, v interface{}) bool {
		key := k.(string)
		methodInfo := v.(*RpcServiceInfo).MethodInfo
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
				for uk, _ := range unUseServiceId {
					mv.Id = uk
					RpcIdMethodMap[mv.Id] = mv
					delete(unUseServiceId, uk)
					break
				}
				continue
			}
			if mv.Id == math.MaxUint64 {
				MessageIdFileMaxId += 1
				mv.Id = MessageIdFileMaxId
			}
			RpcIdMethodMap[mv.Id] = mv
			if FileMaxMessageId < mv.Id && mv.Id != math.MaxUint64 {
				FileMaxMessageId = mv.Id
			}
		}
	}
}

func GetSortServiceList() (ServiceList []string) {
	for k, _ := range ServiceMethodMap {
		ServiceList = append(ServiceList, k)
	}
	sort.Strings(ServiceList)
	return ServiceList
}

func writeGlobalServiceInfoFile() {
	defer util.Wg.Done()
	var includeData = "#include <array>\n"
	includeData += "#include \"service.h\"\n"
	var classHandlerData = ""
	var initFuncData = "std::unordered_set<uint32_t> g_c2s_service_id;\n" +
		"std::array<RpcService, " + strconv.FormatUint(MessageIdLen(), 10) + "> g_message_info;\n\n"

	initFuncData += "void InitMessageInfo()\n{\n"
	ServiceList := GetSortServiceList()
	for _, key := range ServiceList {
		methodList := ServiceMethodMap[key]
		if len(methodList) <= 0 {
			continue
		}
		includeData += methodList[0].IncludeName()
		serviceHandlerName := key + "Impl"
		classHandlerData += "class " + serviceHandlerName + " final : public " + key + "{};\n"
	}
	initFuncData += "\n"
	for _, key := range ServiceList {
		v := ServiceMethodMap[key]
		for i := 0; i < len(v); i++ {
			rpcMethodInfo := v[i]
			rpcId := rpcMethodInfo.KeyName() + config.MessageIdName
			initFuncData += "extern const uint32_t " + rpcId + ";\n"
			serviceHandlerName := key + "Impl"
			cppValue := "g_message_info[" + rpcId
			initFuncData += cppValue + "] = RpcService{" +
				"\"" + rpcMethodInfo.Service + "\"," +
				"\"" + rpcMethodInfo.Method + "\"," +
				"\"" + rpcMethodInfo.Request + "\"," +
				"\"" + rpcMethodInfo.Response + "\"," +
				"std::make_unique_for_overwrite<" + serviceHandlerName + ">()};\n"
			if strings.Contains(rpcId, config.C2SMethodContainsName) {
				initFuncData += "g_c2s_service_id.emplace(" + rpcId + ");\n"
			}
		}
		initFuncData += "\n"
	}
	includeData += "\n"
	classHandlerData += "\n"
	initFuncData += "}\n"
	var data = includeData + classHandlerData + initFuncData

	WriteMd5Data2File(config.ServiceCppFileName, data)
}

func writeGlobalServiceInfoHeadFile() {
	defer util.Wg.Done()
	var data = "#pragma once\n#include <memory>\n" +
		"#include <string>\n" +
		"#include <array>\n" +
		"#include <google/protobuf/message.h>\n" +
		"#include <google/protobuf/service.h>\n\n" +
		"struct RpcService\n{\n" +
		config.Tab + "const char* service{nullptr};\n" +
		config.Tab + "const char* method{nullptr};\n    " +
		config.Tab + "const char* request{nullptr};\n    " +
		config.Tab + "const char* response{nullptr};\n   " +
		" std::unique_ptr<::google::protobuf::Service> service_impl_instance_;\n};\n\n" +
		"using MessageUniquePtr = std::unique_ptr<google::protobuf::Message>;\n\n" +
		"void InitMessageInfo();\n\n" +
		"extern std::array<RpcService, " + strconv.FormatUint(MessageIdLen(), 10) + "> g_message_info;\n\n" +
		"extern std::unordered_set<uint32_t> g_c2s_service_id;\n"

	WriteMd5Data2File(config.ServiceHeadFileName, data)
}

func writeGsGlobalPlayerServiceInstanceFile() {
	defer util.Wg.Done()
	data := ""
	includeData := "#include <memory>\n#include <unordered_map>\n#include \"player_service.h\"\n\n"
	instanceData := ""
	classData := ""
	ServiceList := GetSortServiceList()
	for _, key := range ServiceList {
		methodList, ok := ServiceMethodMap[key]
		if !ok {
			continue
		}

		if !isGsPlayerHandler(&methodList) {
			continue
		}
		method1Info := methodList[0]
		className := method1Info.Service + "Impl"
		includeData += config.IncludeBegin + method1Info.FileBaseName() + config.HeadHandlerEx + config.IncludeEndLine
		classData += "class " + className + " : public " + method1Info.Service + "{};\n"
		instanceData += config.Tab + "g_player_service.emplace(\"" + method1Info.Service +
			"\", std::make_unique<" + method1Info.Service + config.HandlerName + ">(new " +
			className + "));\n"
	}
	data += includeData
	data += "std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_service;\n\n"
	data += classData
	data += "void InitPlayerService()\n{\n"
	data += instanceData
	data += "}"
	WriteMd5Data2File(config.GsMethodHandleDir+config.PlayerServiceName, data)
}

func writeControllerGlobalPlayerServiceInstanceFile() {
	defer util.Wg.Done()
	data := ""
	includeData := "#include <memory>\n#include <unordered_map>\n#include \"player_service.h\"\n\n"
	instanceData := ""
	classData := ""
	ServiceList := GetSortServiceList()
	for _, key := range ServiceList {
		methodList, ok := ServiceMethodMap[key]
		if !ok {
			continue
		}
		if !isControllerPlayerHandler(&methodList) {
			continue
		}
		method1Info := methodList[0]

		className := method1Info.Service + "Impl"
		includeData += config.IncludeBegin + method1Info.FileBaseName() + config.HeadHandlerEx + config.IncludeEndLine
		classData += "class " + className + " : public " + method1Info.Service + "{};\n"
		instanceData += config.Tab + "g_player_service.emplace(\"" + method1Info.Service +
			"\", std::make_unique<" + method1Info.Service + config.HandlerName + ">(new " +
			className + "));\n"
	}
	data += includeData
	data += "std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_service;\n\n"
	data += classData
	data += "void InitPlayerService()\n{\n"
	data += instanceData
	data += "}"
	WriteMd5Data2File(config.CentreMethodHandleDir+config.PlayerServiceName, data)
}

func writeGsGlobalPlayerServiceRepliedInstanceFile() {
	defer util.Wg.Done()
	data := ""
	includeData := "#include <memory>\n#include <unordered_map>\n#include \"player_service_replied.h\"\n\n"
	instanceData := ""
	classData := ""
	ServiceList := GetSortServiceList()
	for _, key := range ServiceList {
		methodList, ok := ServiceMethodMap[key]
		if !ok {
			continue
		}
		if len(methodList) <= 0 {
			continue

		}
		method1Info := methodList[0]
		if !isGsPlayerRepliedHandler(&methodList) {
			continue
		}
		className := method1Info.Service + "Impl"
		includeData += method1Info.CppRepliedHandlerIncludeName()
		classData += "class " + className + " : public " + method1Info.Service + "{};\n"
		instanceData += config.Tab + "g_player_service_replied.emplace(\"" + method1Info.Service +
			"\", std::make_unique<" + method1Info.Service + config.RepliedHandlerName + ">(new " +
			className + "));\n"
	}
	data += includeData
	data += "std::unordered_map<std::string, std::unique_ptr<PlayerServiceReplied>> g_player_service_replied;\n\n"
	data += classData
	data += "void InitPlayerServiceReplied()\n{\n"
	data += instanceData
	data += "}"
	WriteMd5Data2File(config.GsMethodRepliedHandleDir+config.PlayerRepliedServiceName, data)
}

func writeControllerGlobalPlayerServiceRepliedInstanceFile() {
	defer util.Wg.Done()
	data := ""
	includeData := "#include <memory>\n#include <unordered_map>\n#include \"player_service_replied.h\"\n\n"
	instanceData := ""
	classData := ""
	ServiceList := GetSortServiceList()
	for _, key := range ServiceList {
		methodList, ok := ServiceMethodMap[key]
		if !ok {
			continue
		}
		if len(methodList) <= 0 {
			continue
		}
		method1Info := methodList[0]
		if !isControllerPlayerRepliedHandler(&methodList) {
			continue
		}
		className := method1Info.Service + "Impl"
		includeData += method1Info.CppRepliedHandlerIncludeName()
		classData += "class " + className + " : public " + method1Info.Service + "{};\n"
		instanceData += config.Tab + "g_player_service_replied.emplace(\"" + method1Info.Service +
			"\", std::make_unique<" + method1Info.Service + config.RepliedHandlerName + ">(new " +
			className + "));\n"
	}
	data += includeData
	data += "std::unordered_map<std::string, std::unique_ptr<PlayerServiceReplied>> g_player_service_replied;\n\n"
	data += classData
	data += "void InitPlayerServiceReplied()\n{\n"
	data += instanceData
	data += "}"
	WriteMd5Data2File(config.CentreMethodRepliedHandleDir+config.PlayerRepliedServiceName, data)
}

func WriteServiceHandlerFile() {
	util.Wg.Add(1)
	go writeGlobalServiceInfoFile()
	util.Wg.Add(1)
	go writeGlobalServiceInfoHeadFile()
	util.Wg.Add(1)
	writeGsGlobalPlayerServiceInstanceFile()
	util.Wg.Add(1)
	writeControllerGlobalPlayerServiceInstanceFile()
	util.Wg.Add(1)
	writeGsGlobalPlayerServiceRepliedInstanceFile()
	util.Wg.Add(1)
	writeControllerGlobalPlayerServiceRepliedInstanceFile()
}
