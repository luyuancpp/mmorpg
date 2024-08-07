package gen

import (
	"pbgen/config"
	"pbgen/util"
	"strconv"
	"strings"
	"sync"
)

// getClientMethodHandlerHeadStr 生成客户端方法处理器头文件字符串
func getClientMethodHandlerHeadStr(methodList RpcMethodInfos) string {
	var data strings.Builder
	data.WriteString("#pragma once\n")
	data.WriteString("#include <sol/sol.hpp>\n")
	data.WriteString("#include \"thread_local/storage_lua.h\"\n")
	data.WriteString(methodList[0].IncludeName() + "\n")
	data.WriteString("class " + methodList[0].Service + config.HandlerName + " : public ::" + methodList[0].Service + " {\n")
	data.WriteString("public:\n")
	data.WriteString(config.Tab + "void CallMethod(const ::google::protobuf::MethodDescriptor* method,\n")
	data.WriteString(config.Tab + "                 ::google::protobuf::RpcController* controller,\n")
	data.WriteString(config.Tab + "                 const ::google::protobuf::Message* request,\n")
	data.WriteString(config.Tab + "                 ::google::protobuf::Message* response,\n")
	data.WriteString(config.Tab + "                 ::google::protobuf::Closure* done) override {\n")
	data.WriteString(config.Tab2 + "switch(method->index()) {\n")

	for i, method := range methodList {
		data.WriteString(config.Tab3 + "case " + strconv.Itoa(i) + ":\n")
		data.WriteString(config.Tab3 + "{\n")
		data.WriteString(config.Tab4 + "tls_lua_state[\"" + method.KeyName() + config.HandlerName + "\"](\n")
		data.WriteString(config.Tab4 + "    ::google::protobuf::internal::DownCast<const ::" + method.Request + "*>(request),\n")
		data.WriteString(config.Tab4 + "    ::google::protobuf::internal::DownCast<::" + method.Response + "*>(response));\n")
		data.WriteString(config.Tab3 + "}\n")
		data.WriteString(config.Tab3 + "break;\n")
	}
	data.WriteString(config.Tab3 + "default:\n")
	data.WriteString(config.Tab4 + "GOOGLE_LOG(FATAL) << \"Bad method index; this should never happen.\"\n")
	data.WriteString(config.Tab3 + "break;\n")
	data.WriteString(config.Tab2 + "}\n")
	data.WriteString(config.Tab + "}\n")
	data.WriteString("};\n")
	return data.String()
}

// isClientMethodRepliedHandler 检查是否为客户端方法已响应处理器
func isClientMethodRepliedHandler(methodList *RpcMethodInfos) bool {
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

// writeClientMethodHandlerHeadFile 写入客户端方法处理器头文件
func writeClientMethodHandlerHeadFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if len(methodList) <= 0 || !isClientMethodRepliedHandler(&methodList) {
		return
	}
	fileName := methodList[0].FileBaseName() + config.HeadHandlerEx
	WriteMd5Data2File(config.ClientMethodHandleDir+fileName, getClientMethodHandlerHeadStr(methodList))
}

// writeClientHandlerDefaultInstanceFile 写入客户端处理器默认实例文件
func writeClientHandlerDefaultInstanceFile() {
	defer util.Wg.Done()
	var data strings.Builder
	var includeData strings.Builder
	var instanceData strings.Builder

	ServiceList := GetSortServiceList()
	for _, key := range ServiceList {
		methodList, ok := ServiceMethodMap[key]
		if !ok || len(methodList) <= 0 || !isClientMethodRepliedHandler(&methodList) {
			continue
		}
		method1Info := methodList[0]
		includeData.WriteString(config.IncludeBegin + method1Info.FileBaseName() + config.HeadHandlerEx + config.IncludeEndLine)
		instanceData.WriteString(config.Tab + "g_player_service.emplace(\"" + method1Info.Service +
			"\", std::make_unique<" + method1Info.Service + config.HandlerName + ">());\n")
	}
	data.WriteString(includeData.String())
	data.WriteString("std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> g_player_service;\n")
	data.WriteString("void InitPlayerService() {\n")
	data.WriteString(instanceData.String())
	data.WriteString("}\n")
	WriteMd5Data2File(config.ClientServiceInstanceFile, data.String())
}

// WriteClientServiceHeadHandlerFile 写入客户端服务头处理器文件
func WriteClientServiceHeadHandlerFile() {
	var wg sync.WaitGroup
	for _, v := range ServiceMethodMap {
		wg.Add(1)
		go func(methodList RpcMethodInfos) {
			defer wg.Done()
			writeClientMethodHandlerHeadFile(methodList)
		}(v)
	}
	wg.Add(1)
	go func() {
		defer wg.Done()
		writeClientHandlerDefaultInstanceFile()
	}()
	wg.Wait()
}
