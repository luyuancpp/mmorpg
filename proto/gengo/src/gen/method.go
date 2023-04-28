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
	var data = config.ProtoPbhIncludeBegin + s[0].ServiceInfo.FileBaseName() + "_service" + config.HeadEx + config.IncludeEndLine

	for i := 0; i < len(s); i++ {
		data += "const uint32_t " + s[i].KeyName() + config.RpcIdName + " = " + strconv.FormatUint(s[i].Id, 10) + ";\n"
		data += "const uint32_t " + s[i].KeyName() + "Index = " + strconv.FormatUint(s[i].Index, 10) + ";\n"
	}
	fileName := s[0].ServiceInfo.FileBaseName() + "_service" + config.CppEx
	Md5WriteData2File(config.PbcOutDir+fileName, data)
}

func writeMethodHandleHeadFile(s RpcMethodInfos) {
	defer util.Wg.Done()
	if len(s) <= 0 {
		return
	}
	var data = "#pragma once\n"
	data += config.ProtoPbhIncludeBegin + s[0].ServiceInfo.FileBaseName() + config.ProtoPbhIncludeEndLine
	data += "class " + s[0].Service + "Handler : public ::" + s[0].Service + "\n{\npublic:\n"
	for i := 0; i < len(s); i++ {
		data += config.Tab + "void " + s[i].Method + config.GoogleMethodController + "\n" +
			config.Tab + config.Tab + "const ::" + s[i].Request + "* request,\n" +
			config.Tab + config.Tab + "::" + s[i].Response + "* response,\n" +
			config.Tab + config.Tab + " ::google::protobuf::Closure* done)override;\n\n"
	}
	data += "};\n\n"
	fileName := s[0].ServiceInfo.FileBaseName() + "_handler" + config.HeadEx
	Md5WriteData2File(config.GsMethodHandleDir+fileName, data)
}

func writeMethodHandleCppFile(s RpcMethodInfos) {
	defer util.Wg.Done()
}

func writeMethodRepliedHandleCppFile(s RpcMethodInfos) {
	defer util.Wg.Done()
}

func WriteMethodFile() {
	for _, v := range ServiceMethodMap {
		util.Wg.Add(1)
		go writeMethodHeadFile(v)
		util.Wg.Add(1)
		go writeMethodCppFile(v)
		util.Wg.Add(1)
		go writeMethodHandleHeadFile(v)
		util.Wg.Add(1)
		go writeMethodHandleCppFile(v)
		util.Wg.Add(1)
		go writeMethodRepliedHandleCppFile(v)
	}
}
