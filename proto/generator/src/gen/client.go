package gen

import (
	"gengo/config"
	"gengo/util"
	"strconv"
	"strings"
)

func getClientMethodHandlerHeadStr(methodList RpcMethodInfos) string {
	var data = "#pragma once\n" + "#include <sol/sol.hpp>\n" +
		"#include \"src/thread_local/thread_local_storage_lua.h\"\n"
	data += methodList[0].IncludeName() + "\n"
	data += "class " + methodList[0].Service + config.HandlerName + " : public ::" + methodList[0].Service + "\n{\npublic:\n"
	data += config.Tab + "void CallMethod(const ::google::protobuf::MethodDescriptor* method,\n" +
		config.Tab + "::google::protobuf::RpcController* controller,\n" +
		config.Tab + "const ::google::protobuf::Message* request,\n" +
		config.Tab + "::google::protobuf::Message* response,\n" +
		config.Tab + "::google::protobuf::Closure* done)override\n" +
		config.Tab + "{\n" + config.Tab2 + " switch(method->index())\n" +
		config.Tab2 + "{\n"
	for i := 0; i < len(methodList); i++ {
		data += config.Tab3 + "case " + strconv.Itoa(i) + ":\n" + config.Tab3 + "{\n" +
			config.Tab4 + "tls_lua_state[\"" + methodList[i].KeyName() + config.HandlerName + "\"](\n" +
			config.Tab4 + "::google::protobuf::internal::DownCast<const ::" + methodList[i].Request + "*>( request),\n" +
			config.Tab4 + "::google::protobuf::internal::DownCast<::" + methodList[i].Response + "*>(response));\n" +
			config.Tab3 + "}\n" +
			config.Tab3 + "break;\n"
	}
	data += config.Tab3 + "default:\n" +
		config.Tab4 + "GOOGLE_LOG(FATAL) << \"Bad method index; this should never happen.\"\n;" +
		config.Tab3 + "break;\n" + config.Tab2 + "};\n" + config.Tab + "};\n" + "};\n"
	return data
}

func isClientMethodRepliedHandler(methodList *RpcMethodInfos) (check bool) {
	if len(*methodList) <= 0 {
		return false
	}
	firstMethodInfo := (*methodList)[0]
	if strings.Contains(firstMethodInfo.Path, config.ProtoDirNames[config.ClientPlayerDirIndex]) {
		return true
	}
	if strings.Contains(firstMethodInfo.Path, config.ProtoDirNames[config.CommonProtoDirIndex]) {
		return strings.Contains(firstMethodInfo.FileBaseName(), config.LoginPrefixName)
	}
	return false
}

func writeClientMethodHandlerHeadFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if len(methodList) <= 0 {
		return
	}
	if !isClientMethodRepliedHandler(&methodList) {
		return
	}
	fileName := methodList[0].FileBaseName() + config.HeadHandlerEx
	Md5WriteData2File(config.ClientMethodHandleDir+fileName, getClientMethodHandlerHeadStr(methodList))
}

func writeClientHandlerDefaultInstanceFile() {
	defer util.Wg.Done()
	data := ""
	includeData := ""
	instanceData := ""
	ServiceList := GetSortServiceList()
	for _, key := range ServiceList {
		methodList, ok := ServiceMethodMap[key]
		if !ok {
			continue
		}
		if len(methodList) <= 0 {
			continue
		}
		method1Info := methodList[0]
		if !isClientMethodRepliedHandler(&methodList) {
			continue
		}
		includeData += config.IncludeBegin + method1Info.FileBaseName() + config.HeadHandlerEx + config.IncludeEndLine
		instanceData += config.Tab + "g_player_service.emplace(\"" + method1Info.Service +
			"\", std::make_unique<" + method1Info.Service + config.HandlerName + ">());\n"
	}
	data += includeData
	data += "std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> g_player_service;\n"
	data += "void InitPlayerService()\n{\n"
	data += instanceData
	data += "}"
	Md5WriteData2File(config.ClientServiceInstanceFile, data)
}

func WriteClientServiceHeadHandlerFile() {
	for _, v := range ServiceMethodMap {
		util.Wg.Add(1)
		go writeClientMethodHandlerHeadFile(v)
	}
	util.Wg.Add(1)
	go writeClientHandlerDefaultInstanceFile()
}
