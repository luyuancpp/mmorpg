package gen

import (
	"gengo/config"
	"gengo/util"
	"strconv"
	"strings"
)

type checkRepliedCb func(methodList *RpcMethodInfos) bool

func writeServiceMethodHeadFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()

	if len(methodList) <= 0 {
		return
	}
	var data = "#pragma once\n#include <cstdint>\n\n"
	data += methodList[0].IncludeName() + "\n"
	for i := 0; i < len(methodList); i++ {
		data += "extern const uint32_t " + methodList[i].KeyName() + config.MessageIdName + ";\n"
		data += "extern const uint32_t " + methodList[i].KeyName() + "Index;\n"
		data += "#define " + methodList[i].KeyName() + "Method  ::" + methodList[i].Service + "_Stub::descriptor()->method(" +
			strconv.FormatUint(methodList[i].Index, 10) + ")\n"
		data += "\n"
	}
	fileName := methodList[0].FileBaseName() + "_service" + config.HeadEx
	Md5WriteData2File(config.PbcOutDir+fileName, data)
}

func writeServiceMethodCppFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()

	if len(methodList) <= 0 {
		return
	}
	var data = config.IncludeBegin + methodList[0].FileBaseName() + "_service" + config.HeadEx + config.IncludeEndLine

	for i := 0; i < len(methodList); i++ {
		data += "const uint32_t " + methodList[i].KeyName() + config.MessageIdName + " = " + strconv.FormatUint(methodList[i].Id, 10) + ";\n"
		data += "const uint32_t " + methodList[i].KeyName() + "Index = " + strconv.FormatUint(methodList[i].Index, 10) + ";\n"
	}
	fileName := methodList[0].FileBaseName() + "_service" + config.CppEx
	Md5WriteData2File(config.PbcOutDir+fileName, data)
}

func getMethodHandlerHeadStr(methodList RpcMethodInfos) string {
	var data = "#pragma once\n"
	data += methodList[0].IncludeName()
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

func getPlayerMethodHeadStr(methodList RpcMethodInfos) string {
	var data = "#pragma once\n"
	data += methodList[0].IncludeName()
	data += config.PlayerServiceIncludeName
	data += "\nclass " + methodList[0].Service + config.HandlerName + " : public ::PlayerService" + "\n{\npublic:\n"
	data += config.Tab + "using PlayerService::PlayerService;\n"
	var functionNameList string
	var callFunctionList = " void CallMethod(const ::google::protobuf::MethodDescriptor* method,\n   " +
		"entt::entity player,\n    " +
		"const ::google::protobuf::Message* request,\n    " +
		"::google::protobuf::Message* response)override \n " +
		config.Tab2 + "{\n        switch(method->index())\n" +
		config.Tab2 + "{\n"

	for i := 0; i < len(methodList); i++ {
		rq := methodList[i].Request
		rsp := methodList[i].Response
		functionNameList += config.Tab + "static void " + methodList[i].Method + config.PlayerMethodController + "\n" +
			config.Tab2 + "const ::" + rq + "* request,\n" +
			config.Tab2 + "::" + rsp + "* response);\n\n"
		callFunctionList += config.Tab2 + "case " + strconv.Itoa(i) + ":\n"
		callFunctionList += config.Tab3 + methodList[i].Method + "(player,\n"
		callFunctionList += config.Tab3 + "::google::protobuf::internal::DownCast<const " + rq + "*>( request),\n"
		callFunctionList += config.Tab3 + "::google::protobuf::internal::DownCast<" + rsp + "*>(response));\n"
		callFunctionList += config.Tab2 + "break;\n"
	}
	callFunctionList += config.Tab2 + "default:\n" +
		//config.Tab3 + "GOOGLE_LOG(FATAL) << \"Bad method index; this should never happen.\";\n" +
		config.Tab2 + "break;\n" + config.Tab2 + "}\n" + config.Tab + "}\n"
	data += functionNameList
	data += callFunctionList
	data += "\n};\n"
	return data
}

func getPlayerMethodRepliedHeadStr(methodList RpcMethodInfos) string {
	var data = "#pragma once\n"
	data += methodList[0].IncludeName()
	data += config.PlayerServiceRepliedIncludeName
	data += "\nclass " + methodList[0].Service + config.RepliedHandlerName + " : public ::PlayerServiceReplied" + "\n{\npublic:\n"
	data += config.Tab + "using PlayerServiceReplied::PlayerServiceReplied;\n"
	var functionNameList string
	var callFunctionList = " void CallMethod(const ::google::protobuf::MethodDescriptor* method,\n   " +
		"entt::entity player,\n    " +
		"const ::google::protobuf::Message* request,\n    " +
		"::google::protobuf::Message* response)override \n " +
		config.Tab2 + "{\n        switch(method->index())\n" +
		config.Tab2 + "{\n"

	for i := 0; i < len(methodList); i++ {
		rq := methodList[i].Request
		rsp := methodList[i].Response
		functionNameList += config.Tab + "static void " + methodList[i].Method + config.PlayerMethodController + "\n" +
			config.Tab2 + "const ::" + rq + "* request,\n" +
			config.Tab2 + "::" + rsp + "* response);\n\n"
		callFunctionList += config.Tab2 + "case " + strconv.Itoa(i) + ":\n"
		callFunctionList += config.Tab3 + methodList[i].Method + "(player,\n"
		callFunctionList += config.Tab3 + "nullptr,\n"
		callFunctionList += config.Tab3 + "::google::protobuf::internal::DownCast<" + rsp + "*>(response));\n"
		callFunctionList += config.Tab2 + "break;\n"
	}
	callFunctionList += config.Tab2 + "default:\n" +
		//config.Tab3 + "GOOGLE_LOG(FATAL) << \"Bad method index; this should never happen.\";\n" +
		config.Tab2 + "break;\n" + config.Tab2 + "}\n" + config.Tab + "}\n"
	data += functionNameList
	data += callFunctionList
	data += "\n};\n"
	return data
}

func getMethodRepliedHandlerHeadStr(methodList *RpcMethodInfos) (data string) {
	methodLen := len(*methodList)
	firstMethodInfo := (*methodList)[0]
	data = firstMethodInfo.IncludeName() +
		"#include \"muduo/net/TcpConnection.h\"\n" +
		"using namespace muduo;\n" +
		"using namespace muduo::net;\n\n"

	for i := 0; i < methodLen; i++ {
		methodInfo := (*methodList)[i]

		data += "void On" + methodInfo.KeyName() + config.RepliedHandlerName + "(const TcpConnectionPtr& conn, const " +
			"std::shared_ptr<" + methodInfo.Response + ">& replied, Timestamp timestamp);\n\n"
	}
	return data
}

func getMethodHandlerCppStr(dst string, methodList *RpcMethodInfos) (data string) {
	methodLen := len(*methodList)
	yourCodes, _ := util.GetDstCodeData(dst, methodLen+1)
	firstMethodInfo := (*methodList)[0]
	data = firstMethodInfo.CppHandlerIncludeName() +
		"#include \"src/game_logic/thread_local/thread_local_storage.h\"\n" +
		"#include \"src/network/message_system.h\"\n"

	className := firstMethodInfo.Service + config.HandlerName
	for i := 0; i < len(yourCodes); i++ {
		j := i - 1
		isMessage := j >= 0 && j < methodLen
		if isMessage {
			methodInfo := (*methodList)[j]
			data += "void " + className + "::" + methodInfo.Method + config.GoogleMethodController + "\n" +
				config.Tab + "const ::" + methodInfo.Request + "* request,\n" +
				config.Tab + "::" + methodInfo.Response + "* response,\n" +
				config.Tab + " ::google::protobuf::Closure* done)\n{\n"
		}
		data += yourCodes[i]
		if isMessage {
			data += "}\n\n"
		}
	}
	return data
}

func getMethodRepliedHandlerCppStr(dst string, methodList *RpcMethodInfos) (data string) {
	methodLen := len(*methodList)
	yourCodes, _ := util.GetDstCodeData(dst, methodLen+1)
	firstMethodInfo := (*methodList)[0]
	data = firstMethodInfo.CppRepliedHandlerIncludeName() +
		"#include \"src/network/codec/dispatcher.h\"\n\n"

	implData := ""
	declarationData := ""

	for i := 0; i < len(yourCodes); i++ {
		j := i - 1
		isMessage := j >= 0 && j < methodLen
		if isMessage {
			break
		}
		data += yourCodes[i]
	}
	data += "extern ProtobufDispatcher g_response_dispatcher;\n\n"
	for i := 0; i < len(yourCodes); i++ {
		j := i - 1
		isMessage := j >= 0 && j < methodLen
		if isMessage {
			methodInfo := (*methodList)[j]

			funcName := "On" + methodInfo.KeyName() + config.RepliedHandlerName
			declarationData += config.Tab + "g_response_dispatcher.registerMessageCallback<" +
				methodInfo.Response + ">(std::bind(&" + funcName +
				", std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));\n"
			implData += "void " + funcName + "(const TcpConnectionPtr& conn, const " +
				"std::shared_ptr<" + methodInfo.Response + ">& replied, Timestamp timestamp)\n{\n"
			implData += yourCodes[i]
			implData += "}\n\n"
		}
	}
	data += "\nvoid Init" + firstMethodInfo.KeyName() + config.RepliedHandlerName + "()\n{\n"
	data += declarationData
	data += "}\n\n"

	data += implData

	for i := 0; i < len(yourCodes); i++ {
		j := i - 1
		if j < methodLen {
			continue
		}
		data += yourCodes[i]
	}

	return data
}

func getMethodPlayerHandlerCppStr(dst string, methodList *RpcMethodInfos, className string, includeName string) (data string) {
	methodLen := len(*methodList)
	yourCodes, _ := util.GetDstCodeData(dst, methodLen+1)
	data = includeName +
		"#include \"src/game_logic/thread_local/thread_local_storage.h\"\n" +
		"#include \"src/network/message_system.h\"\n"
	for i := 0; i < len(yourCodes); i++ {
		j := i - 1
		isMessage := j >= 0 && j < methodLen
		if isMessage {
			methodInfo := (*methodList)[j]
			data += "void " + className + "::" + methodInfo.Method + config.PlayerMethodController + "\n" +
				config.Tab + "const ::" + methodInfo.Request + "* request,\n" +
				config.Tab + "::" + methodInfo.Response + "* response)\n{\n"
		}
		data += yourCodes[i]
		if isMessage {
			data += "}\n\n"
		}
	}
	return data
}

func writeRegisterFile(dst string, cb checkRepliedCb) {
	defer util.Wg.Done()
	data := ""
	instanceData := ""

	ServiceList := GetSortServiceList()
	for _, key := range ServiceList {
		methodList, ok := ServiceMethodMap[key]
		if !ok {
			continue
		}
		if !cb(&methodList) {
			continue
		}
		firstMethodInfo := methodList[0]
		data += firstMethodInfo.CppHandlerIncludeName()
		instanceData += config.Tab + "g_server_service.emplace(\"" + firstMethodInfo.Service +
			"\", std::make_unique_for_overwrite<" + firstMethodInfo.Service + config.HandlerName + ">());\n"
	}
	data += "\nstd::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> g_server_service;\n\n"

	data += "void InitServiceHandler()\n{\n"
	data += instanceData
	data += "}"
	Md5WriteData2File(dst, data)
}

func writeRepliedRegisterFile(dst string, cb checkRepliedCb) {
	defer util.Wg.Done()
	data := "void InitRepliedHandler()\n{\n"
	ServiceList := GetSortServiceList()
	for _, key := range ServiceList {
		methodList, ok := ServiceMethodMap[key]
		if !ok {
			continue
		}
		if !cb(&methodList) {
			continue
		}
		firstMethodInfo := methodList[0]
		data += config.Tab + "void Init" + firstMethodInfo.KeyName() + config.RepliedHandlerName + "();\n"
		data += config.Tab + "Init" + firstMethodInfo.KeyName() + config.RepliedHandlerName + "();\n\n"
	}
	data += "}\n"
	Md5WriteData2File(dst, data)
}

// / game server
func isGsMethodHandler(methodList *RpcMethodInfos) (isGsFile bool) {
	if len(*methodList) <= 0 {
		return
	}
	firstMethodList := (*methodList)[0]
	if !(strings.Contains(firstMethodList.Path, config.ProtoDirNames[config.CommonProtoDirIndex]) ||
		strings.Contains(firstMethodList.Path, config.ProtoDirNames[config.LogicProtoDirIndex])) {
		return false
	}
	return strings.Contains(firstMethodList.FileBaseName(), config.GsPrefixName)
}

func isGsPlayerHandler(methodList *RpcMethodInfos) (result bool) {
	if len(*methodList) <= 0 {
		return false
	}
	firstMethodInfo := (*methodList)[0]
	if !firstMethodInfo.IsPlayerService() {
		return false
	}
	if !strings.Contains(firstMethodInfo.FileBaseName(), config.PlayerName) {
		return false
	}
	return true
}

func writeGsMethodHandlerHeadFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()

	if !isGsMethodHandler(&methodList) {
		return
	}
	fileName := methodList[0].FileBaseName() + config.HeadHandlerEx
	Md5WriteData2File(config.GsMethodHandleDir+fileName, getMethodHandlerHeadStr(methodList))
}

func writeGsMethodHandlerCppFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if !isGsMethodHandler(&methodList) {
		return
	}
	fileName := strings.ToLower(methodList[0].FileBaseName()) + config.CppHandlerEx
	dstFileName := config.GsMethodHandleDir + fileName
	data := getMethodHandlerCppStr(dstFileName, &methodList)
	Md5WriteData2File(dstFileName, data)
}

func writeGsPlayerMethodHandlerHeadFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if !isGsPlayerHandler(&methodList) {
		return
	}
	fileName := methodList[0].FileBaseName() + config.HeadHandlerEx
	Md5WriteData2File(config.GsMethodHandleDir+fileName, getPlayerMethodHeadStr(methodList))
}

func writeGsPlayerMethodHandlerCppFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if len(methodList) <= 0 {
		return
	}
	if !isGsPlayerHandler(&methodList) {
		return
	}
	firstMethodInfo := methodList[0]
	fileName := strings.ToLower(methodList[0].FileBaseName()) + config.CppHandlerEx
	dstFileName := config.GsMethodHandleDir + fileName
	data := getMethodPlayerHandlerCppStr(dstFileName,
		&methodList,
		firstMethodInfo.CppHandlerClassName(),
		firstMethodInfo.CppHandlerIncludeName())
	Md5WriteData2File(dstFileName, data)
}

func isGsPlayerRepliedHandler(methodList *RpcMethodInfos) (result bool) {
	if len(*methodList) <= 0 {
		return
	}
	firstMethodInfo := (*methodList)[0]
	if !firstMethodInfo.IsPlayerService() {
		return false
	}
	return !strings.Contains(firstMethodInfo.FileBaseName(), config.GsPrefixName)
}

func writeGsPlayerMethodRepliedHandlerHeadFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if !isGsPlayerRepliedHandler(&methodList) {
		return
	}
	fileName := methodList[0].FileBaseName() + config.HeadRepliedHandlerEx
	Md5WriteData2File(config.GsMethodRepliedHandleDir+fileName, getPlayerMethodRepliedHeadStr(methodList))
}

func writeGsPlayerMethodRepliedHandlerCppFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if !isGsPlayerRepliedHandler(&methodList) {
		return
	}
	firstMethodInfo := methodList[0]
	fileName := strings.ToLower(methodList[0].FileBaseName()) + config.CppRepliedHandlerEx
	dstFileName := config.GsMethodRepliedHandleDir + fileName
	data := getMethodPlayerHandlerCppStr(dstFileName,
		&methodList,
		firstMethodInfo.CppRepliedHandlerClassName(),
		firstMethodInfo.CppRepliedHandlerIncludeName())
	Md5WriteData2File(dstFileName, data)
}

func isGsMethodRepliedHandler(methodList *RpcMethodInfos) (check bool) {
	if len(*methodList) <= 0 {
		return false
	}
	firstMethodInfo := (*methodList)[0]
	if !(strings.Contains(firstMethodInfo.Path, config.ProtoDirNames[config.CommonProtoDirIndex]) ||
		strings.Contains(firstMethodInfo.Path, config.ProtoDirNames[config.LogicProtoDirIndex])) {
		return false
	}
	return strings.Contains(firstMethodInfo.FileBaseName(), "deploy") ||
		strings.Contains(firstMethodInfo.FileBaseName(), config.ControllerPrefixName) ||
		strings.Contains(firstMethodInfo.FileBaseName(), "lobby")
}

func writeGsMethodRepliedHandlerHeadFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if len(methodList) <= 0 {
		return
	}
	fileBaseName := methodList[0].FileBaseName()
	if !isGsMethodRepliedHandler(&methodList) {
		return
	}

	fileName := strings.ToLower(fileBaseName) + config.HeadRepliedHandlerEx
	dstFileName := config.GsMethodRepliedHandleDir + fileName
	data := getMethodRepliedHandlerHeadStr(&methodList)
	Md5WriteData2File(dstFileName, data)
}

func writeGsMethodRepliedHandlerCppFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if len(methodList) <= 0 {
		return
	}
	fileBaseName := methodList[0].FileBaseName()
	if !isGsMethodRepliedHandler(&methodList) {
		return
	}

	fileName := strings.ToLower(fileBaseName) + config.CppRepliedHandlerEx
	dstFileName := config.GsMethodRepliedHandleDir + fileName
	data := getMethodRepliedHandlerCppStr(dstFileName, &methodList)
	Md5WriteData2File(dstFileName, data)
}

// controller server

func isControllerMethodHandler(methodList *RpcMethodInfos) (isGsFile bool) {
	if len(*methodList) <= 0 {
		return false
	}
	firstMethodInfo := (*methodList)[0]
	if !(strings.Contains(firstMethodInfo.Path, config.ProtoDirNames[config.CommonProtoDirIndex]) ||
		strings.Contains(firstMethodInfo.Path, config.ProtoDirNames[config.LogicProtoDirIndex])) {
		return false
	}
	return strings.Contains(firstMethodInfo.FileBaseName(), config.ControllerPrefixName)
}

func writeControllerMethodHandlerHeadFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if !isControllerMethodHandler(&methodList) {
		return
	}
	fileName := methodList[0].FileBaseName() + config.HeadHandlerEx
	Md5WriteData2File(config.ControllerMethodHandleDir+fileName, getMethodHandlerHeadStr(methodList))
}

func isControllerPlayerHandler(methodList *RpcMethodInfos) (result bool) {
	if len(*methodList) <= 0 {
		return false
	}
	firstMethodInfo := (*methodList)[0]
	if !firstMethodInfo.IsPlayerService() {
		return false
	}
	if !strings.Contains(firstMethodInfo.FileBaseName(), config.ControllerPrefixName) {
		return false
	}
	return true
}

func writeControllerPlayerMethodHandlerHeadFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if !isControllerPlayerHandler(&methodList) {
		return
	}
	fileName := methodList[0].FileBaseName() + config.HeadHandlerEx
	Md5WriteData2File(config.ControllerMethodHandleDir+fileName, getPlayerMethodHeadStr(methodList))
}

func writeControllerPlayerMethodHandlerCppFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if !isControllerPlayerHandler(&methodList) {
		return
	}
	firstMethodInfo := methodList[0]
	fileName := strings.ToLower(methodList[0].FileBaseName()) + config.CppHandlerEx
	dstFileName := config.ControllerMethodHandleDir + fileName
	data := getMethodPlayerHandlerCppStr(dstFileName,
		&methodList,
		firstMethodInfo.CppHandlerClassName(),
		firstMethodInfo.CppHandlerIncludeName())
	Md5WriteData2File(dstFileName, data)
}

func writeControllerMethodHandlerCppFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if !isControllerMethodHandler(&methodList) {
		return
	}

	fileName := strings.ToLower(methodList[0].FileBaseName()) + config.CppHandlerEx
	dstFileName := config.ControllerMethodHandleDir + fileName
	data := getMethodHandlerCppStr(dstFileName, &methodList)
	Md5WriteData2File(dstFileName, data)
}

func isControllerMethodRepliedHandler(methodList *RpcMethodInfos) (check bool) {
	if len(*methodList) <= 0 {
		return false
	}
	firstMethodInfo := (*methodList)[0]
	if !(strings.Contains(firstMethodInfo.Path, config.ProtoDirNames[config.CommonProtoDirIndex]) ||
		strings.Contains(firstMethodInfo.Path, config.ProtoDirNames[config.LogicProtoDirIndex])) {
		return false
	}
	return !strings.Contains(firstMethodInfo.FileBaseName(), config.ControllerPrefixName)
}

func writeControllerMethodRepliedHandlerHeadFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()

	if !isControllerMethodRepliedHandler(&methodList) {
		return
	}
	fileName := strings.ToLower(methodList[0].FileBaseName()) + config.HeadRepliedHandlerEx
	dstFileName := config.ControllerMethodRepliedHandleDir + fileName
	data := getMethodRepliedHandlerHeadStr(&methodList)
	Md5WriteData2File(dstFileName, data)
}

func writeControllerMethodRepliedHandlerCppFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if !isControllerMethodRepliedHandler(&methodList) {
		return
	}
	firstMethodInfo := methodList[0]
	fileName := strings.ToLower(firstMethodInfo.FileBaseName()) + config.CppRepliedHandlerEx
	dstFileName := config.ControllerMethodRepliedHandleDir + fileName
	data := getMethodRepliedHandlerCppStr(dstFileName, &methodList)
	Md5WriteData2File(dstFileName, data)
}

func isControllerPlayerRepliedHandler(methodList *RpcMethodInfos) (result bool) {
	if len(*methodList) <= 0 {
		return
	}
	firstMethodInfo := (*methodList)[0]
	if !firstMethodInfo.IsPlayerService() {
		return false
	}
	return !strings.Contains(firstMethodInfo.FileBaseName(), config.ControllerPrefixName)
}

func writeControllerPlayerMethodRepliedHandlerHeadFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if !isControllerPlayerRepliedHandler(&methodList) {
		return
	}
	fileName := methodList[0].FileBaseName() + config.HeadRepliedHandlerEx
	Md5WriteData2File(config.ControllerMethodRepliedHandleDir+fileName, getPlayerMethodRepliedHeadStr(methodList))
}

func writeControllerPlayerMethodRepliedHandlerCppFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if !isControllerPlayerRepliedHandler(&methodList) {
		return
	}
	firstMethodInfo := methodList[0]
	fileName := strings.ToLower(methodList[0].FileBaseName()) + config.CppRepliedHandlerEx
	dstFileName := config.ControllerMethodRepliedHandleDir + fileName
	data := getMethodPlayerHandlerCppStr(dstFileName,
		&methodList,
		firstMethodInfo.CppRepliedHandlerClassName(),
		firstMethodInfo.CppRepliedHandlerIncludeName())
	Md5WriteData2File(dstFileName, data)
}

// /gate
func isGateMethodRepliedHandler(methodList *RpcMethodInfos) (check bool) {
	if len(*methodList) <= 0 {
		return false
	}
	firstMethodInfo := (*methodList)[0]
	if !strings.Contains(firstMethodInfo.Path, config.ProtoDirNames[config.CommonProtoDirIndex]) {
		return false
	}
	return strings.Contains(firstMethodInfo.FileBaseName(), config.ControllerPrefixName) ||
		strings.Contains(firstMethodInfo.FileBaseName(), "deploy") ||
		strings.Contains(firstMethodInfo.FileBaseName(), "lobby") ||
		strings.Contains(firstMethodInfo.FileBaseName(), config.GsPrefixName)
}

func isGateServiceHandler(methodList *RpcMethodInfos) (check bool) {
	if len(*methodList) <= 0 {
		return false
	}
	firstMethodInfo := (*methodList)[0]
	if !strings.Contains(firstMethodInfo.Path, config.ProtoDirNames[config.CommonProtoDirIndex]) {
		return false
	}
	return strings.Contains(firstMethodInfo.FileBaseName(), "gate")
}

func writeGateMethodHandlerHeadFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()

	if len(methodList) <= 0 {
		return
	}
	if !isGateServiceHandler(&methodList) {
		return
	}
	fileName := methodList[0].FileBaseName() + config.HeadHandlerEx
	Md5WriteData2File(config.GateMethodHandleDir+fileName, getMethodHandlerHeadStr(methodList))
}

func writeGateMethodHandlerCppFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if len(methodList) <= 0 {
		return
	}
	if !isGateServiceHandler(&methodList) {
		return
	}
	fileName := strings.ToLower(methodList[0].FileBaseName()) + config.CppHandlerEx
	dstFileName := config.GateMethodHandleDir + fileName
	data := getMethodHandlerCppStr(dstFileName, &methodList)
	Md5WriteData2File(dstFileName, data)
}

func writeGateMethodRepliedHandlerHeadFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if len(methodList) <= 0 {
		return
	}
	if !isGateMethodRepliedHandler(&methodList) {
		return
	}
	fileName := strings.ToLower(methodList[0].FileBaseName()) + config.HeadRepliedHandlerEx
	dstFileName := config.GateMethodRepliedHandleDir + fileName
	data := getMethodRepliedHandlerHeadStr(&methodList)
	Md5WriteData2File(dstFileName, data)
}

func writeGateMethodRepliedHandlerCppFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if !isGateMethodRepliedHandler(&methodList) {
		return
	}
	firstMethodInfo := methodList[0]
	fileName := strings.ToLower(firstMethodInfo.FileBaseName()) + config.CppRepliedHandlerEx
	dstFileName := config.GateMethodRepliedHandleDir + fileName
	data := getMethodRepliedHandlerCppStr(dstFileName, &methodList)
	Md5WriteData2File(dstFileName, data)
}

///login

func isLoginMethodRepliedHandler(methodList *RpcMethodInfos) (check bool) {
	if len(*methodList) <= 0 {
		return false
	}
	firstMethodInfo := (*methodList)[0]
	if !strings.Contains(firstMethodInfo.Path, config.ProtoDirNames[config.CommonProtoDirIndex]) {
		return false
	}
	return strings.Contains(firstMethodInfo.FileBaseName(), config.ControllerPrefixName) ||
		strings.Contains(firstMethodInfo.FileBaseName(), "deploy")
}

func isLoginServiceHandler(methodList *RpcMethodInfos) (check bool) {
	if len(*methodList) <= 0 {
		return false
	}
	firstMethodInfo := (*methodList)[0]
	if !strings.Contains(firstMethodInfo.Path, config.ProtoDirNames[config.CommonProtoDirIndex]) {
		return false
	}
	return strings.Contains(firstMethodInfo.FileBaseName(), "login")
}

func writeLoginMethodHandlerHeadFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()

	if len(methodList) <= 0 {
		return
	}
	if !isLoginServiceHandler(&methodList) {
		return
	}
	fileName := methodList[0].FileBaseName() + config.HeadHandlerEx
	Md5WriteData2File(config.LoginMethodHandleDir+fileName, getMethodHandlerHeadStr(methodList))
}

func writeLoginMethodHandlerCppFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if len(methodList) <= 0 {
		return
	}
	if !isLoginServiceHandler(&methodList) {
		return
	}
	fileName := strings.ToLower(methodList[0].FileBaseName()) + config.CppHandlerEx
	dstFileName := config.LoginMethodHandleDir + fileName
	data := getMethodHandlerCppStr(dstFileName, &methodList)
	Md5WriteData2File(dstFileName, data)
}

func writeLoginMethodRepliedHandlerHeadFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if len(methodList) <= 0 {
		return
	}

	if !isLoginMethodRepliedHandler(&methodList) {
		return
	}
	fileName := strings.ToLower(methodList[0].FileBaseName()) + config.HeadRepliedHandlerEx
	dstFileName := config.LoginMethodRepliedHandleDir + fileName
	data := getMethodRepliedHandlerHeadStr(&methodList)
	Md5WriteData2File(dstFileName, data)
}

func writeLoginMethodRepliedHandlerCppFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	methodLen := len(methodList)
	if methodLen <= 0 {
		return
	}

	if !isLoginMethodRepliedHandler(&methodList) {
		return
	}
	firstMethodInfo := methodList[0]
	fileName := strings.ToLower(firstMethodInfo.FileBaseName()) + config.CppRepliedHandlerEx
	dstFileName := config.LoginMethodRepliedHandleDir + fileName
	data := getMethodRepliedHandlerCppStr(dstFileName, &methodList)
	Md5WriteData2File(dstFileName, data)
}

func WriteMethodFile() {
	for _, v := range ServiceMethodMap {
		util.Wg.Add(1)
		go writeServiceMethodHeadFile(v)
		util.Wg.Add(1)
		go writeServiceMethodCppFile(v)

		//gs
		util.Wg.Add(1)
		go writeGsMethodHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeGsMethodHandlerCppFile(v)
		util.Wg.Add(1)
		go writeGsPlayerMethodHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeGsPlayerMethodHandlerCppFile(v)
		util.Wg.Add(1)
		go writeGsMethodRepliedHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeGsMethodRepliedHandlerCppFile(v)
		util.Wg.Add(1)
		go writeGsPlayerMethodRepliedHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeGsPlayerMethodRepliedHandlerCppFile(v)

		//Controller
		util.Wg.Add(1)
		go writeControllerMethodHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeControllerMethodHandlerCppFile(v)
		util.Wg.Add(1)
		go writeControllerPlayerMethodHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeControllerPlayerMethodHandlerCppFile(v)
		util.Wg.Add(1)
		go writeControllerMethodRepliedHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeControllerMethodRepliedHandlerCppFile(v)
		util.Wg.Add(1)
		go writeControllerPlayerMethodRepliedHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeControllerPlayerMethodRepliedHandlerCppFile(v)

		//gate
		util.Wg.Add(1)
		go writeGateMethodHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeGateMethodHandlerCppFile(v)
		util.Wg.Add(1)
		go writeGateMethodRepliedHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeGateMethodRepliedHandlerCppFile(v)

		// login
		util.Wg.Add(1)
		go writeLoginMethodHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeLoginMethodHandlerCppFile(v)
		util.Wg.Add(1)
		go writeLoginMethodRepliedHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeLoginMethodRepliedHandlerCppFile(v)

		//

	}
	//game
	util.Wg.Add(1)
	go writeRegisterFile(config.GsMethodHandleDir+config.RegisterHandlerCppEx, isGsMethodHandler)
	util.Wg.Add(1)
	go writeRepliedRegisterFile(config.GsMethodRepliedHandleDir+config.RegisterRepliedHandlerCppEx, isGsMethodRepliedHandler)

	//controller
	util.Wg.Add(1)
	go writeRegisterFile(config.ControllerMethodHandleDir+config.RegisterHandlerCppEx, isControllerMethodHandler)
	util.Wg.Add(1)
	go writeRepliedRegisterFile(config.ControllerMethodRepliedHandleDir+config.RegisterRepliedHandlerCppEx, isControllerMethodRepliedHandler)

	//login
	util.Wg.Add(1)
	go writeRepliedRegisterFile(config.LoginMethodRepliedHandleDir+config.RegisterRepliedHandlerCppEx, isLoginMethodRepliedHandler)

	//gate
	util.Wg.Add(1)
	go writeRepliedRegisterFile(config.GateMethodRepliedHandleDir+config.RegisterRepliedHandlerCppEx, isGateMethodRepliedHandler)
}
