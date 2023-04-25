package gen

import (
	"bufio"
	"fmt"
	"gengo/config"
	"gengo/util"
	"log"
	"os"
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

func PrintAll() {
	rpcMethod.Range(func(key, value interface{}) bool {
		fmt.Println("Key =", key, "Value =", value)
		return true
	})
}

func InitServiceId() {
	var unUseServiceId = map[uint64]struct{}{}
	maxServiceId := uint64(1)

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
		if newMethodValue.Id <= 0 {
			maxServiceId += 1
			newMethodValue.Id = maxServiceId
		}
		rpcIdMethod[newMethodValue.Id] = newMethodValue
		rpcMethod.Swap(key, newMethodValue)
		return true
	})
}

func serviceImpl() {
	defer util.Wg.Done()
	var data string
	for i := 0; i < len(rpcIdMethod); i++ {
		rpcMethodInfo := rpcIdMethod[uint64(i)]
		data += strconv.FormatUint(rpcMethodInfo.Id, 10) + "=" + rpcMethodInfo.KeyName() + "\n"
	}
	os.WriteFile(config.ServiceIdsFileName, []byte(data), 0666)
}

func ServiceImpl() {
	util.Wg.Add(1)
	go serviceImpl()
}
