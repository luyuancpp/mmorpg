package gen

import (
	"gengo/config"
	"gengo/util"
	"strconv"
	"strings"
)

func writeMethodHeadFile(pMethodList *RpcMethodInfos) {

	defer util.Wg.Done()

	s := *pMethodList
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

func writeMethodCppFile(pMethodList *RpcMethodInfos) {

	defer util.Wg.Done()

	s := *pMethodList
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

func getMethodHandlerHeadStr(pMethodList *RpcMethodInfos) string {
	methodList := *pMethodList
	var data = "#pragma once\n"
	data += config.ProtoPbhIncludeBegin + methodList[0].ServiceInfo.FileBaseName() + config.ProtoPbhIncludeEndLine
	data += "class " + methodList[0].Service + "Handler : public ::" + methodList[0].Service + "\n{\npublic:\n"
	for i := 0; i < len(methodList); i++ {
		data += config.Tab + "void " + methodList[i].Method + config.GoogleMethodController + "\n" +
			config.Tab2 + "const ::" + methodList[i].Request + "* request,\n" +
			config.Tab2 + "::" + methodList[i].Response + "* response,\n" +
			config.Tab2 + " ::google::protobuf::Closure* done)override;\n\n"
	}
	data += "};\n\n"
	return data
}

func writeGsMethodHandlerHeadFile(pMethodList *RpcMethodInfos) {

	defer util.Wg.Done()

	methodList := *pMethodList
	if len(methodList) <= 0 {
		return
	}
	if strings.Contains(methodList[0].ServiceInfo.FileBaseName(), config.PlayerName) {
		return
	}
	if methodList[0].ServiceInfo.Path != config.ProtoDirNames[config.LogicProtoDirIndex] {
		if !strings.Contains(methodList[0].ServiceInfo.FileBaseName(), "game") {
			return
		}
	}
	fileName := methodList[0].ServiceInfo.FileBaseName() + "_handler" + config.HeadEx
	Md5WriteData2File(config.GsMethodHandleDir+fileName, getMethodHandlerHeadStr(pMethodList))
}

func writeMethodHandlerCppFile(s RpcMethodInfos) {
	defer util.Wg.Done()

	if strings.Contains(s[0].ServiceInfo.FileBaseName(), config.PlayerName) {
		return
	}
}

func writeMethodRepliedHandleCppFile(s RpcMethodInfos) {
	defer util.Wg.Done()
	if strings.Contains(s[0].ServiceInfo.FileBaseName(), config.PlayerName) {
		return
	}
}

func writePlayerMethodHandlerHeadFile(s RpcMethodInfos) {
	defer util.Wg.Done()
	if len(s) <= 0 {
		return
	}
	if !strings.Contains(s[0].ServiceInfo.FileBaseName(), config.PlayerName) {
		return
	}
	var data = "#pragma once\n"
	data += config.ProtoPbhIncludeBegin + s[0].ServiceInfo.FileBaseName() + config.ProtoPbhIncludeEndLine
	data += config.PlayerServiceIncludeName
	data += "\nclass " + s[0].Service + config.HandlerName + " : public ::PlayerService" + "\n{\npublic:\n"
	data += config.Tab + "PlayerService::PlayerService;\n"
	var functionNameList string
	var callFunctionList = " void CallMethod(const ::google::protobuf::MethodDescriptor* method,\n   " +
		"entt::entity player,\n    " +
		"const ::google::protobuf::Message* request,\n    " +
		"::google::protobuf::Message* response)override \n " +
		" {\n        switch(method->index()) {\n"

	for i := 0; i < len(s); i++ {
		rq := s[i].Request
		rsp := s[i].Response
		functionNameList += config.Tab + "void " + s[i].Method + config.PlayerMethodController + "\n" +
			config.Tab2 + "const ::" + rq + "* request,\n" +
			config.Tab2 + "::" + rsp + "* response);\n\n"
		callFunctionList += config.Tab2 + "case " + strconv.Itoa(i) + ":\n"
		callFunctionList += config.Tab3 + s[i].Method + "(player,\n"
		callFunctionList += config.Tab3 + "::google::protobuf::internal::DownCast<const " + rq + "*>( request),\n"
		callFunctionList += config.Tab3 + "::google::protobuf::internal::DownCast<" + rsp + "*>(response));\n"
		callFunctionList += config.Tab2 + "break;\n"
	}
	callFunctionList += config.Tab2 + "default:\n" +
		config.Tab3 + "GOOGLE_LOG(FATAL) << \"Bad method index; this should never happen.\";\n" +
		config.Tab2 + "break;\n" + config.Tab2 + "}\n" + config.Tab + "}\n"
	data += functionNameList
	data += callFunctionList
	data += "};\n\n"
	fileName := s[0].ServiceInfo.FileBaseName() + "_handler" + config.HeadEx
	Md5WriteData2File(config.GsMethodHandleDir+fileName, data)
}

func writePlayerMethodHandlerCppFile(s RpcMethodInfos) {
	defer util.Wg.Done()
	if len(s) <= 0 {
		return
	}
	if !strings.Contains(s[0].ServiceInfo.FileBaseName(), config.PlayerName) {
		return
	}
}

func writePlayerMethodRepliedHandleCppFile(s RpcMethodInfos) {
	defer util.Wg.Done()
	if len(s) <= 0 {
		return
	}
	if !strings.Contains(s[0].ServiceInfo.FileBaseName(), config.PlayerName) {
		return
	}
}

func WriteMethodFile() {
	for _, v := range ServiceMethodMap {
		util.Wg.Add(1)
		go writeMethodHeadFile(&v)
		util.Wg.Add(1)
		go writeMethodCppFile(&v)
		util.Wg.Add(1)
		go writeGsMethodHandlerHeadFile(&v)
		util.Wg.Add(1)
		go writeMethodHandlerCppFile(v)
		util.Wg.Add(1)
		go writeMethodRepliedHandleCppFile(v)
		util.Wg.Add(1)
		go writePlayerMethodHandlerHeadFile(v)
		util.Wg.Add(1)
		go writePlayerMethodHandlerCppFile(v)
		util.Wg.Add(1)
		go writePlayerMethodRepliedHandleCppFile(v)
	}
}
