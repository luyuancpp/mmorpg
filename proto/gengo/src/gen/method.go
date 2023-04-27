package gen

import (
	"gengo/config"
	"gengo/util"
	"strconv"
)

func writeMethodHeadFile(s RpcMethodInfos) {
	defer util.Wg.Done()
	if len(s) <= 0 {
		return
	}
	var data = "#pragma once\n#include <cstdint>\n\n"
	data += s[0].ServiceInfo.IncludeName() + "\n"
	for i := 0; i < len(s); i++ {
		data += "extern const uint32_t " + s[i].KeyName() + config.RpcIdName + ";\n"
		data += "extern const uint32_t " + s[i].KeyName() + "Index;\n"
		data += "#define " + s[i].KeyName() + "Method  ::" + s[i].Service + "_Stub::descriptor()->method(" +
			strconv.FormatUint(s[i].Index, 10) + ");\n"
		data += "\n"
	}
	fileName := s[0].ServiceInfo.FileBaseName() + "_service" + config.HeadEx
	Md5WriteData2File(config.PbcOutDir+fileName, data)
}

func writeMethodCppFile(s RpcMethodInfos) {
	defer util.Wg.Done()
	if len(s) <= 0 {
		return
	}
	var data = "#include \"" + s[0].ServiceInfo.FileBaseName() + "_service" + config.HeadEx + "\"\n"

	for i := 0; i < len(s); i++ {
		data += "const uint32_t " + s[i].KeyName() + config.RpcIdName + " = " + strconv.FormatUint(s[i].Id, 10) + ";\n"
		data += "const uint32_t " + s[i].KeyName() + "Index = " + strconv.FormatUint(s[i].Index, 10) + ";\n"
	}
	fileName := s[0].ServiceInfo.FileBaseName() + "_service" + config.CppEx
	Md5WriteData2File(config.PbcOutDir+fileName, data)
}

func WriteMethodFile() {
	for _, v := range ServiceMethods {
		util.Wg.Add(1)
		go writeMethodHeadFile(v)
		util.Wg.Add(1)
		go writeMethodCppFile(v)
	}
}
