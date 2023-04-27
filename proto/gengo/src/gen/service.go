package gen

import (
	"bufio"
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

type RpcServiceInfo struct {
	FileName string
	Path     string
}

type RpcMethodInfo struct {
	Service     string
	Method      string
	Request     string
	Response    string
	Id          uint64
	Index       uint64
	ServiceInfo *RpcServiceInfo
}

type RpcMethodInfos []RpcMethodInfo

var rpcLineReplacer = strings.NewReplacer("(", "", ")", "", ";", "", "\n", "")

var RpcService sync.Map
var RpcMethod sync.Map
var RpcIdMethod = map[uint64]RpcMethodInfo{}
var ServiceId = map[string]uint64{}
var ServiceMethods = map[string]RpcMethodInfos{}

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
	var serviceInfo *RpcServiceInfo
	for scanner.Scan() {
		line = scanner.Text()
		if strings.Contains(line, "service ") && !strings.Contains(line, "=") {
			service = strings.ReplaceAll(line, "{", "")
			service = strings.Split(service, " ")[1]
			var rpcServiceInfo RpcServiceInfo
			rpcServiceInfo.FileName = fd.Name()
			rpcServiceInfo.Path = filePath
			RpcService.Store(service, rpcServiceInfo)
			serviceInfo = &rpcServiceInfo
			continue
		} else if strings.Contains(line, "rpc ") {
			line = rpcLineReplacer.Replace(strings.Trim(line, " "))
			splitList := strings.Split(line, " ")
			var rpcMethodInfo RpcMethodInfo
			rpcMethodInfo.Service = service
			rpcMethodInfo.Method = splitList[1]
			rpcMethodInfo.Request = splitList[2]
			rpcMethodInfo.Response = splitList[4]
			rpcMethodInfo.Id = math.MaxUint64
			rpcMethodInfo.Index = methodIndex
			rpcMethodInfo.ServiceInfo = serviceInfo
			methodIndex += 1
			RpcMethod.Store(rpcMethodInfo.KeyName(), rpcMethodInfo)
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
		ServiceId[splitList[1]], err = strconv.ParseUint(splitList[0], 10, 32)
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
	for i := 0; i < len(RpcIdMethod); i++ {
		rpcMethodInfo := RpcIdMethod[uint64(i)]
		data += strconv.FormatUint(rpcMethodInfo.Id, 10) + "=" + rpcMethodInfo.KeyName() + "\n"
	}
	os.WriteFile(config.ServiceIdsFileName, []byte(data), 0666)
}

func WriteServiceIdFile() {
	util.Wg.Add(1)
	go writeServiceIdFile()
}

func InitServiceId() {
	var unUseServiceId = map[uint64]struct{}{}
	var maxServiceId uint64

	for k, v := range ServiceId {
		if maxServiceId < v {
			maxServiceId = v
		}
		methodValue, ok := RpcMethod.Load(k)
		if !ok {
			unUseServiceId[v] = struct{}{}
			continue
		}
		newMethodValue := methodValue.(RpcMethodInfo)
		newMethodValue.Id = v
		RpcMethod.Swap(newMethodValue.KeyName(), newMethodValue)
	}
	RpcMethod.Range(func(k, v interface{}) bool {
		key := k.(string)
		newMethodValue := v.(RpcMethodInfo)
		for uk, _ := range unUseServiceId {
			newMethodValue.Id = uk
			delete(unUseServiceId, uk)
			break
		}
		if newMethodValue.Id == math.MaxUint64 {
			maxServiceId += 1
			newMethodValue.Id = maxServiceId
		}
		RpcIdMethod[newMethodValue.Id] = newMethodValue
		RpcMethod.Swap(key, newMethodValue)

		if _, ok := ServiceMethods[newMethodValue.Service]; !ok {
			ServiceMethods[newMethodValue.Service] = RpcMethodInfos{}
		}
		ServiceMethods[newMethodValue.Service] = append(ServiceMethods[newMethodValue.Service], newMethodValue)
		return true
	})
	for _, v := range ServiceMethods {
		sort.Sort(v)
	}
}

func writeServiceHandlerFile() {
	defer util.Wg.Done()
	var includeData = "#include <unordered_map>\n"
	includeData += "#include \"service.h\"\n"
	var classHandlerData = ""
	var initFuncData = "std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> g_services;\n\n"
	initFuncData += "std::unordered_map<uint32_t, RpcService> g_service_method_info;\n"
	initFuncData += "void InitService()\n{\n"
	var serviceList []string
	RpcService.Range(func(k, v interface{}) bool {
		key := k.(string)
		serviceList = append(serviceList, key)
		return true
	})
	sort.Strings(serviceList)
	for _, key := range serviceList {
		value, _ := RpcService.Load(key)
		rpcServiceInfo := value.(RpcServiceInfo)
		includeData += rpcServiceInfo.IncludeName()
		serviceHandlerName := key + "Impl"
		classHandlerData += "class " + serviceHandlerName + ":public " + key + "{};\n"
		initFuncData += " g_services.emplace(\"" + key + "\", std::make_unique<" + serviceHandlerName + ">());\n"
	}
	initFuncData += "\n"
	for _, v := range ServiceMethods {
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
