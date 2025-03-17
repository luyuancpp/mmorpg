package gen

import (
	"pbgen/config"
	"pbgen/util"
	"strconv"
	"strings"
)

// getClientMethodHandlerHeadStr 生成客户端方法处理器头文件字符串
func getClientMethodHandlerHeadStr(methodList RPCMethods) string {
	var builder strings.Builder

	builder.WriteString("#pragma once\n")
	builder.WriteString("#include <sol/sol.hpp>\n")
	builder.WriteString("#include \"thread_local/storage_lua.h\"\n")
	builder.WriteString(methodList[0].IncludeName() + "\n")
	builder.WriteString("class " + methodList[0].Service + config.HandlerFileName + " : public ::" + methodList[0].Service + " {\n")
	builder.WriteString("public:\n")
	builder.WriteString(config.Tab + "void CallMethod(const ::google::protobuf::MethodDescriptor* method,\n")
	builder.WriteString(config.Tab + "                 ::google::protobuf::RpcController* controller,\n")
	builder.WriteString(config.Tab + "                 const ::google::protobuf::Message* request,\n")
	builder.WriteString(config.Tab + "                 ::google::protobuf::Message* response,\n")
	builder.WriteString(config.Tab + "                 ::google::protobuf::Closure* done) override {\n")
	builder.WriteString(config.Tab2 + "switch(method->index()) {\n")

	for i, method := range methodList {
		builder.WriteString(config.Tab3 + "case " + strconv.Itoa(i) + ":\n")
		builder.WriteString(config.Tab3 + "{\n")
		builder.WriteString(config.Tab4 + "tls_lua_state[\"" + method.KeyName() + config.HandlerFileName + "\"](\n")
		builder.WriteString(config.Tab4 + "    static_cast<const ::" + method.Request + "*>(request),\n")
		builder.WriteString(config.Tab4 + "    static_cast<::" + method.Response + "*>(response));\n")
		builder.WriteString(config.Tab3 + "}\n")
		builder.WriteString(config.Tab3 + "break;\n")
	}
	builder.WriteString(config.Tab3 + "default:\n")
	builder.WriteString(config.Tab4 + "GOOGLE_LOG(FATAL) << \"Bad method index; this should never happen.\"\n")
	builder.WriteString(config.Tab3 + "break;\n")
	builder.WriteString(config.Tab2 + "}\n")
	builder.WriteString(config.Tab + "}\n")
	builder.WriteString("};\n")
	return builder.String()
}

// isClientMethodRepliedHandler 检查是否为客户端方法已响应处理器
func isClientMethodRepliedHandler(methodList *RPCMethods) bool {
	if len(*methodList) == 0 {
		return false
	}
	firstMethod := (*methodList)[0]
	return strings.Contains(firstMethod.Path, config.ProtoDirectoryNames[config.ClientPlayerDirIndex]) ||
		(strings.Contains(firstMethod.Path, config.ProtoDirectoryNames[config.CommonProtoDirIndex]))
}

// writeClientMethodHandlerHeadFile 写入客户端方法处理器头文件
func writeClientMethodHandlerHeadFile(methodList RPCMethods) {
	if len(methodList) == 0 || !isClientMethodRepliedHandler(&methodList) {
		return
	}

	fileName := methodList[0].FileBaseName() + config.HandlerHeaderExtension
	util.WriteMd5Data2File(config.RobotMethodHandlerDirectory+fileName, getClientMethodHandlerHeadStr(methodList))
}

// writeClientHandlerDefaultInstanceFile 写入客户端处理器默认实例文件
func writeClientHandlerDefaultInstanceFile() {

	var builder strings.Builder
	var includeBuilder strings.Builder
	var instanceBuilder strings.Builder

	ServiceList := GetSortServiceList()

	for _, key := range ServiceList {
		methodList, ok := ServiceMethodMap[key]
		if !ok || len(methodList) == 0 || !isClientMethodRepliedHandler(&methodList) {
			continue
		}
		method1Info := methodList[0]
		includeBuilder.WriteString(config.IncludeBegin + method1Info.FileBaseName() + config.HandlerHeaderExtension + config.IncludeEndLine)
		instanceBuilder.WriteString(config.Tab + "g_player_service.emplace(\"" + method1Info.Service +
			"\", std::make_unique<" + method1Info.Service + config.HandlerFileName + ">());\n")
	}

	builder.WriteString(includeBuilder.String())
	builder.WriteString("std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> g_player_service;\n")
	builder.WriteString("void InitPlayerService() {\n")
	builder.WriteString(instanceBuilder.String())
	builder.WriteString("}\n")

	util.WriteMd5Data2File(config.ClientServiceInstanceFilePath, builder.String())
}

// WriteClientServiceHeadHandlerFile 写入客户端服务头处理器文件
func WriteClientServiceHeadHandlerFile() {
	for _, v := range ServiceMethodMap {
		util.Wg.Add(1)
		go func(methodList RPCMethods) {
			defer util.Wg.Done()
			writeClientMethodHandlerHeadFile(methodList)
		}(v)
	}

	util.Wg.Add(1)
	go func() {
		defer util.Wg.Done()
		writeClientHandlerDefaultInstanceFile()
	}()

	util.Wg.Wait()
}
