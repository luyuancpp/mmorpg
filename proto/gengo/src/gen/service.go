package gen

import (
	"bufio"
	"fmt"
	"gengo/config"
	"gengo/util"
	"log"
	"os"
	"strings"
	"sync"
)

type RpcMethodInfo struct {
	Service  string
	Method   string
	Request  string
	Response string
	Id       uint32
}

var rpcLineReplacer = strings.NewReplacer("(", "", ")", "", ";", "", "\n", "")

var rpcServiceList sync.Map
var rpcMethodList sync.Map

func BuildService(fd os.DirEntry, filePath string) (err error) {
	defer util.Wg.Done()
	f, err := os.Open(filePath + fd.Name())
	if err != nil {
		return
	}
	defer f.Close()
	// Splits on newlines by default.
	scanner := bufio.NewScanner(f)
	// https://golang.org/pkg/bufio/#Scanner.Scan
	var line string
	var service string
	for scanner.Scan() {
		line = scanner.Text()
		if strings.Contains(line, "service ") && !strings.Contains(line, "=") {
			service = strings.ReplaceAll(line, "{", "")
			rpcServiceList.Store(service, "")
			continue
		} else if strings.Contains(line, "rpc ") {
			line = rpcLineReplacer.Replace(strings.Trim(line, " "))
			splitInfo := strings.Split(line, " ")
			var rpcMethodInfo RpcMethodInfo
			rpcMethodInfo.Service = service
			rpcMethodInfo.Method = splitInfo[1]
			rpcMethodInfo.Request = splitInfo[2]
			rpcMethodInfo.Response = splitInfo[4]
			rpcMethodList.Store(rpcMethodInfo.Method, rpcMethodInfo)
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

func BuildAllService() {
	for i := 0; i < len(config.ProtoDirs); i++ {
		fds, err := os.ReadDir(config.ProtoDirs[i])
		if err != nil {
			continue
		}
		for _, fd := range fds {
			if !util.IsProtoFile(fd) {
				continue
			}
			util.Wg.Add(1)
			BuildService(fd, config.ProtoDirs[i])
		}
	}
}

func walk(key, value interface{}) bool {
	fmt.Println("Key =", key, "Value =", value)
	return true
}

func PrintAll() {
	rpcServiceList.Range(walk)
	rpcMethodList.Range(walk)
}
