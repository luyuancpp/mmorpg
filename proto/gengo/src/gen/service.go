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

var rpcLineReplacer = strings.NewReplacer("(", "", ")", "", ";", "", "\n", "")

var rpcService sync.Map
var rpcMethod sync.Map
var serviceId = map[string]uint64{}
var unUseServiceId = map[uint64]struct{}{}
var maxServiceId uint64

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
			rpcService.Store(service, "")
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

func ReadAllServices() {
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

func ReadServiceIdFile() {
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

func walk(key, value interface{}) bool {
	fmt.Println("Key =", key, "Value =", value)
	return true
}

func initMethodId(k, v interface{}) bool {
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
	rpcMethod.Swap(key, newMethodValue)
	return true
}

func PrintAll() {
	rpcMethod.Range(walk)
}

func InitServiceId() {
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
	rpcMethod.Range(initMethodId)
	PrintAll()
}
