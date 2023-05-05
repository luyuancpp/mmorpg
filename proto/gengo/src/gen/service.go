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
	"sync"
)

type EmptyStruct struct{}

type RpcMethodInfo struct {
	Service  string
	Method   string
	Request  string
	Response string
	Id       uint64
	Index    uint64
	FileName string
	Path     string
}

type RpcMethodInfos []*RpcMethodInfo

type RpcServiceInfo struct {
	FileName   string
	Path       string
	MethodInfo RpcMethodInfos
}

var rpcLineReplacer = strings.NewReplacer("(", "", ")", "", ";", "", "\n", "")

var RpcServiceSyncMap sync.Map
var RpcIdMethodMap = map[uint64]*RpcMethodInfo{}
var ServiceIdMap = map[string]uint64{}
var ServiceList []string
var ServiceMethodMap = map[string]RpcMethodInfos{}

func (info *RpcMethodInfo) KeyName() (idName string) {
	return info.Service + info.Method
}

func (info *RpcServiceInfo) IncludeName() (includeName string) {
	return "#include \"" + strings.Replace(info.Path, config.ProtoDir, "", 1) + info.PbcHeadName() + "\"\n"
}

func (info *RpcServiceInfo) PbcHeadName() (pbcHeadName string) {
	return strings.Replace(info.FileName, config.ProtoEx, config.ProtoPbhEx, 1)
}

func (info *RpcServiceInfo) HeadName() (headName string) {
	return strings.Replace(info.FileName, config.ProtoEx, config.HeadEx, 1)
}

func (info *RpcServiceInfo) FileBaseName() (fileBaseName string) {
	return strings.Replace(info.FileName, config.ProtoEx, "", 1)
}

func (info *RpcMethodInfo) FileBaseName() (fileBaseName string) {
	return strings.Replace(info.FileName, config.ProtoEx, "", 1)
}

func (info *RpcMethodInfo) PbcHeadName() (pbcHeadName string) {
	return strings.Replace(info.FileName, config.ProtoEx, config.ProtoPbhEx, 1)
}

func (info *RpcMethodInfo) IncludeName() (includeName string) {
	return config.IncludeBegin + strings.Replace(info.Path, config.ProtoDir, "", 1) + info.PbcHeadName() + "\"\n"
}

func (info *RpcMethodInfo) CppHandlerIncludeName() (includeName string) {
	return config.IncludeBegin + info.FileBaseName() + "_handler" + config.HeadEx + config.IncludeEndLine
}

func (s RpcMethodInfos) Len() int {
	return len(s)
}

func (s RpcMethodInfos) Less(i, j int) bool {
	if s[i].Service < s[j].Service {
		return true
	}
	return s[i].Index < s[j].Index
}

func (s RpcMethodInfos) Swap(i, j int) {
	s[i], s[j] = s[j], s[i]
}

func ReadProtoFileService(fd os.DirEntry, filePath string) (err error) {
	defer util.Wg.Done()
	f, err := os.Open(filePath + fd.Name())
	if err != nil {
		return
	}
	defer f.Close()

	scanner := bufio.NewScanner(f)
	var line string
	var service string
	var methodIndex uint64
	var rpcServiceInfo RpcServiceInfo
	for scanner.Scan() {
		line = scanner.Text()
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
			rpcMethodInfo.Request = splitList[2]
			rpcMethodInfo.Response = strings.Replace(splitList[4], ".", "::", -1)
			rpcMethodInfo.Id = math.MaxUint64
			rpcMethodInfo.Index = methodIndex
			rpcMethodInfo.FileName = fd.Name()
			rpcMethodInfo.Path = filePath
			rpcServiceInfo.MethodInfo = append(rpcServiceInfo.MethodInfo, &rpcMethodInfo)

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
		ServiceIdMap[splitList[1]], err = strconv.ParseUint(splitList[0], 10, 32)
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
	for i := 0; i < len(RpcIdMethodMap); i++ {
		rpcMethodInfo, ok := RpcIdMethodMap[uint64(i)]
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
	var maxServiceId uint64
	var curMethodCount = 0

	RpcServiceSyncMap.Range(func(k, v interface{}) bool {
		key := k.(string)
		methodInfo := v.(*RpcServiceInfo).MethodInfo
		ServiceMethodMap[key] = methodInfo
		return true
	})

	for _, methodList := range ServiceMethodMap {
		curMethodCount += len(methodList)
		for _, mv := range methodList {
			id, ok := ServiceIdMap[mv.KeyName()]
			//Id文件未找到则是新消息，新消息后面处理，这里不处理
			if !ok {
				continue
			}
			useServiceId[id] = EmptyStruct{}
			mv.Id = id
		}
	}

	for _, v := range ServiceIdMap {
		if _, ok := useServiceId[v]; !ok {
			unUseServiceId[v] = EmptyStruct{}
		}
		if maxServiceId < v {
			maxServiceId = v
		}
	}

	for _, methodList := range ServiceMethodMap {
		curMethodCount += len(methodList)
		for _, mv := range methodList {
			for uk, _ := range unUseServiceId {
				mv.Id = uk
				useServiceId[uk] = EmptyStruct{}
				delete(unUseServiceId, uk)
				break
			}
			if mv.Id == math.MaxUint64 {
				mv.Id = maxServiceId
				maxServiceId += 1
			}
			RpcIdMethodMap[mv.Id] = mv
		}
	}
}

func writeServiceHandlerFile() {
	defer util.Wg.Done()
	var includeData = "#include <unordered_map>\n"
	includeData += "#include \"service.h\"\n"
	var classHandlerData = ""
	var initFuncData = "std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> g_services;\n\n"
	initFuncData += "std::unordered_map<uint32_t, RpcService> g_service_method_info;\n\n"
	initFuncData += "void InitService()\n{\n"

	for k, _ := range ServiceMethodMap {
		ServiceList = append(ServiceList, k)
	}
	sort.Strings(ServiceList)
	for _, key := range ServiceList {
		methodList := ServiceMethodMap[key]
		if len(methodList) > 0 {
			includeData += methodList[0].IncludeName()
		}
		serviceHandlerName := key + "Impl"
		classHandlerData += "class " + serviceHandlerName + ":public " + key + "{};\n"
		initFuncData += " g_services.emplace(\"" + key + "\", std::make_unique<" + serviceHandlerName + ">());\n"
	}
	initFuncData += "\n"
	for _, key := range ServiceList {
		v := ServiceMethodMap[key]
		for i := 0; i < len(v); i++ {
			rpcMethodInfo := v[i]
			rpcId := rpcMethodInfo.KeyName() + config.RpcIdName
			initFuncData += "extern const uint32_t " + rpcId + ";\n"
			cppValue := "g_service_method_info[" + rpcId
			initFuncData += cppValue + "] = RpcService{" +
				"\"" + rpcMethodInfo.Service + "\"," +
				"\"" + rpcMethodInfo.Method + "\"," +
				"\"" + rpcMethodInfo.Request + "\"," +
				"\"" + rpcMethodInfo.Response + "\"};\n"
		}
		initFuncData += "\n"
	}
	includeData += "\n"
	classHandlerData += "\n"
	initFuncData += "}\n"
	var data = includeData + classHandlerData + initFuncData

	Md5WriteData2File(config.ServiceFileName, data)
}

func WriteServiceHandlerFile() {
	util.Wg.Add(1)
	go writeServiceHandlerFile()
}
