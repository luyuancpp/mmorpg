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

type RpcMethodInfo struct {
	Service  string
	Method   string
	Request  string
	Response string
	Id       uint64
}

type RpcServiceInfo struct {
	FileName string
	Path     string
}

var rpcLineReplacer = strings.NewReplacer("(", "", ")", "", ";", "", "\n", "")

var rpcService sync.Map
var rpcMethod sync.Map
var rpcIdMethod = map[uint64]RpcMethodInfo{}
var serviceId = map[string]uint64{}

func (info *RpcMethodInfo) KeyName() (idName string) {
	return info.Service + info.Method
}

func (info *RpcServiceInfo) IncludeDir() (idName string) {
	return strings.Replace(info.Path, config.ProtoDir, "", 1)
}

func (info *RpcServiceInfo) CppHeadName() (idName string) {
	return strings.Replace(info.FileName, config.ProtoEx, config.ProtoPbhEx, 1)
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
	for scanner.Scan() {
		line = scanner.Text()
		if strings.Contains(line, "service ") && !strings.Contains(line, "=") {
			service = strings.ReplaceAll(line, "{", "")
			service = strings.Split(service, " ")[1]
			var rpcServiceInfo RpcServiceInfo
			rpcServiceInfo.FileName = fd.Name()
			rpcServiceInfo.Path = filePath
			rpcService.Store(service, rpcServiceInfo)
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
			rpcMethod.Store(rpcMethodInfo.KeyName(), rpcMethodInfo)
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
		serviceId[splitList[1]], err = strconv.ParseUint(splitList[0], 10, 32)
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
	for i := 0; i < len(rpcIdMethod); i++ {
		rpcMethodInfo := rpcIdMethod[uint64(i)]
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

	for k, v := range serviceId {
		if maxServiceId < v {
			maxServiceId = v
		}
		methodValue, ok := rpcMethod.Load(k)
		if !ok {
			unUseServiceId[v] = struct{}{}
			continue
		}
		newMethodValue := methodValue.(RpcMethodInfo)
		newMethodValue.Id = v
		rpcMethod.Swap(newMethodValue.KeyName(), newMethodValue)
	}
	rpcMethod.Range(func(k, v interface{}) bool {
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
		rpcIdMethod[newMethodValue.Id] = newMethodValue
		rpcMethod.Swap(key, newMethodValue)
		return true
	})
}

func writeServiceImplFile() {
	defer util.Wg.Done()
	var includeData = "#include <unordered_map>\n"
	var classImplData = ""
	var initFuncData = "std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> g_services;\n\n"
	initFuncData += "void InitServiceImpl()\n{\n"
	var serviceList []string
	rpcService.Range(func(k, v interface{}) bool {
		key := k.(string)
		serviceList = append(serviceList, key)
		return true
	})
	sort.Strings(serviceList)
	for _, key := range serviceList {
		value, _ := rpcService.Load(key)
		rpcServiceInfo := value.(RpcServiceInfo)
		includeData += "#include \"" + rpcServiceInfo.IncludeDir() + rpcServiceInfo.CppHeadName() + "\"\n"
		serviceImplName := key + "Impl"
		classImplData += "class " + serviceImplName + ":public " + key + "{};\n"
		initFuncData += " g_services.emplace(\"" + key + "\", std::make_unique<" + serviceImplName + ">());\n"
	}
	includeData += "\n"
	classImplData += "\n"
	initFuncData += "}\n"
	var data = includeData + classImplData + initFuncData

	Md5CopyData2File(config.ServiceImplFileName, data)
}

func WriteServiceImplFile() {
	util.Wg.Add(1)
	go writeServiceImplFile()
}

func writeMethodIdFile() {

}

func WriteMethodIdFile() {

}
