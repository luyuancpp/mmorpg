package gen

import (
	"gengo/config"
	"gengo/util"
	"sort"
	"strconv"
)

type RpcMethodInfos []RpcMethodInfo

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

func writeMethodFile(s RpcMethodInfos) {
	defer util.Wg.Done()
	if len(s) <= 0 {
		return
	}
	var data = "#pragma once\n#include <cstdint>\n\n"
	data += s[0].ServiceInfo.IncludeName() + "\n"
	for i := 0; i < len(s); i++ {
		data += "const uint32_t " + s[i].KeyName() + "MsgId = " + strconv.FormatUint(s[i].Id, 10) + ";\n"
	}
	data += "\n"
	fileName := s[0].ServiceInfo.FileBaseName() + "method" + config.HeadEx
	Md5WriteData2File(config.PbcOutDir+fileName, data)
}

func WriteMethodFile() {
	var serviceMethods = map[string]RpcMethodInfos{}
	RpcMethod.Range(func(k, v interface{}) bool {
		value := v.(RpcMethodInfo)
		if _, ok := serviceMethods[value.Service]; !ok {
			serviceMethods[value.Service] = RpcMethodInfos{}
		}
		serviceMethods[value.Service] = append(serviceMethods[value.Service], value)
		return true
	})
	for _, v := range serviceMethods {
		sort.Sort(v)
		util.Wg.Add(1)
		go writeMethodFile(v)
	}
}
