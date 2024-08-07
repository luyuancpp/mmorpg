package gen

import (
	"pbgen/config"
	"pbgen/util"
	"strconv"
	"strings"
)

type checkRepliedCb func(methodList *RPCMethods) bool

func writeServiceIdHeadFile(methodList RPCMethods) {
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
	util.WriteMd5Data2File(config.ServiceDirName+fileName, data)
}

func writeServiceIdCppFile(methodList RPCMethods) {
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
	util.WriteMd5Data2File(config.ServiceDirName+fileName, data)
}

func getServiceHandlerHeadStr(methodList RPCMethods) string {
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

func getPlayerMethodHeadStr(methodList RPCMethods) string {
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

func getPlayerMethodRepliedHeadStr(methodList RPCMethods) string {
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

func getMethodRepliedHandlerHeadStr(methodList *RPCMethods) (data string) {
	methodLen := len(*methodList)
	firstMethodInfo := (*methodList)[0]
	data = "#pragma once\n" + firstMethodInfo.IncludeName() +
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

func getMethodHandlerCppStr(dst string, methodList *RPCMethods) (data string) {
	methodLen := len(*methodList)
	yourCodes, _ := util.ReadCodeSectionsFromFile(dst, methodLen+1)
	firstMethodInfo := (*methodList)[0]
	data = firstMethodInfo.CppHandlerIncludeName()

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

func getMethodRepliedHandlerCppStr(dst string, methodList *RPCMethods) (data string) {
	methodLen := len(*methodList)
	yourCodes, _ := util.ReadCodeSectionsFromFile(dst, methodLen+1)
	firstMethodInfo := (*methodList)[0]
	data = firstMethodInfo.CppRepliedHandlerIncludeName() +
		"#include \"network/codec/dispatcher.h\"\n\n"

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

func getMethodPlayerHandlerCppStr(dst string, methodList *RPCMethods, className string, includeName string) (data string) {
	methodLen := len(*methodList)
	yourCodes, _ := util.ReadCodeSectionsFromFile(dst, methodLen+1)
	data = includeName
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
	util.WriteMd5Data2File(dst, data)
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
	util.WriteMd5Data2File(dst, data)
}

// / game server
func isGsMethodHandler(methodList *RPCMethods) (isGsFile bool) {
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

func isGsPlayerHandler(methodList *RPCMethods) (result bool) {
	if len(*methodList) <= 0 {
		return false
	}
	firstMethodInfo := (*methodList)[0]
	if !firstMethodInfo.IsPlayerService() {
		return false
	}
	if !strings.Contains(firstMethodInfo.FileBaseName(), config.PlayerName) {
		return false
	} else if strings.Contains(firstMethodInfo.FileBaseName(), config.CentrePrefixName) {
		return false
	}
	return true
}

func writeGsMethodHandlerHeadFile(methodList RPCMethods) {
	defer util.Wg.Done()

	if !isGsMethodHandler(&methodList) {
		return
	}
	fileName := methodList[0].FileBaseName() + config.HeadHandlerEx
	util.WriteMd5Data2File(config.GsMethodHandleDir+fileName, getServiceHandlerHeadStr(methodList))
}

func writeGsMethodHandlerCppFile(methodList RPCMethods) {
	defer util.Wg.Done()
	if !isGsMethodHandler(&methodList) {
		return
	}
	fileName := strings.ToLower(methodList[0].FileBaseName()) + config.CppHandlerEx
	dstFileName := config.GsMethodHandleDir + fileName
	data := getMethodHandlerCppStr(dstFileName, &methodList)
	util.WriteMd5Data2File(dstFileName, data)
}

func writeGsPlayerMethodHandlerHeadFile(methodList RPCMethods) {
	defer util.Wg.Done()
	if !isGsPlayerHandler(&methodList) {
		return
	}
	fileName := methodList[0].FileBaseName() + config.HeadHandlerEx
	util.WriteMd5Data2File(config.GsMethodHandleDir+fileName, getPlayerMethodHeadStr(methodList))
}

func writeGsPlayerMethodHandlerCppFile(methodList RPCMethods) {
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
	util.WriteMd5Data2File(dstFileName, data)
}

func isGsPlayerRepliedHandler(methodList *RPCMethods) (result bool) {
	if len(*methodList) <= 0 {
		return
	}
	firstMethodInfo := (*methodList)[0]
	if !firstMethodInfo.IsPlayerService() {
		return false
	}
	return !strings.Contains(firstMethodInfo.FileBaseName(), config.GsPrefixName)
}

func writeGsPlayerMethodRepliedHandlerHeadFile(methodList RPCMethods) {
	defer util.Wg.Done()
	if !isGsPlayerRepliedHandler(&methodList) {
		return
	}
	fileName := methodList[0].FileBaseName() + config.HeadRepliedHandlerEx
	util.WriteMd5Data2File(config.GsMethodRepliedHandleDir+fileName, getPlayerMethodRepliedHeadStr(methodList))
}

func writeGsPlayerMethodRepliedHandlerCppFile(methodList RPCMethods) {
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
	util.WriteMd5Data2File(dstFileName, data)
}

func isGsMethodRepliedHandler(methodList *RPCMethods) (check bool) {
	if len(*methodList) <= 0 {
		return false
	}
	firstMethodInfo := (*methodList)[0]
	if !(strings.Contains(firstMethodInfo.Path, config.ProtoDirNames[config.CommonProtoDirIndex]) ||
		strings.Contains(firstMethodInfo.Path, config.ProtoDirNames[config.LogicProtoDirIndex])) {
		return false
	}
	if !firstMethodInfo.CcGenericServices {
		return false
	}
	return strings.Contains(firstMethodInfo.FileBaseName(), "deploy") ||
		strings.Contains(firstMethodInfo.FileBaseName(), config.CentrePrefixName) ||
		strings.Contains(firstMethodInfo.FileBaseName(), "lobby")
}

func writeGsMethodRepliedHandlerHeadFile(methodList RPCMethods) {
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
	util.WriteMd5Data2File(dstFileName, data)
}

func writeGsMethodRepliedHandlerCppFile(methodList RPCMethods) {
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
	util.WriteMd5Data2File(dstFileName, data)
}

// centre server

func isCentreMethodHandler(methodList *RPCMethods) (isGsFile bool) {
	if len(*methodList) <= 0 {
		return false
	}
	firstMethodInfo := (*methodList)[0]
	if !(strings.Contains(firstMethodInfo.Path, config.ProtoDirNames[config.CommonProtoDirIndex]) ||
		strings.Contains(firstMethodInfo.Path, config.ProtoDirNames[config.LogicProtoDirIndex])) {
		return false
	}
	return strings.Contains(firstMethodInfo.FileBaseName(), config.CentrePrefixName)
}

func writeCentreMethodHandlerHeadFile(methodList RPCMethods) {
	defer util.Wg.Done()
	if !isCentreMethodHandler(&methodList) {
		return
	}
	fileName := methodList[0].FileBaseName() + config.HeadHandlerEx
	util.WriteMd5Data2File(config.CentreMethodHandleDir+fileName, getServiceHandlerHeadStr(methodList))
}

func isCentrePlayerHandler(methodList *RPCMethods) (result bool) {
	if len(*methodList) <= 0 {
		return false
	}
	firstMethodInfo := (*methodList)[0]
	if !firstMethodInfo.IsPlayerService() {
		return false
	}
	if !strings.Contains(firstMethodInfo.FileBaseName(), config.CentrePrefixName) {
		return false
	}
	return true
}

func writeCentrePlayerMethodHandlerHeadFile(methodList RPCMethods) {
	defer util.Wg.Done()
	if !isCentrePlayerHandler(&methodList) {
		return
	}
	fileName := methodList[0].FileBaseName() + config.HeadHandlerEx
	util.WriteMd5Data2File(config.CentreMethodHandleDir+fileName, getPlayerMethodHeadStr(methodList))
}

func writeCentrePlayerMethodHandlerCppFile(methodList RPCMethods) {
	defer util.Wg.Done()
	if !isCentrePlayerHandler(&methodList) {
		return
	}
	firstMethodInfo := methodList[0]
	fileName := strings.ToLower(methodList[0].FileBaseName()) + config.CppHandlerEx
	dstFileName := config.CentreMethodHandleDir + fileName
	data := getMethodPlayerHandlerCppStr(dstFileName,
		&methodList,
		firstMethodInfo.CppHandlerClassName(),
		firstMethodInfo.CppHandlerIncludeName())
	util.WriteMd5Data2File(dstFileName, data)
}

func writeCentreMethodHandlerCppFile(methodList RPCMethods) {
	defer util.Wg.Done()
	if !isCentreMethodHandler(&methodList) {
		return
	}

	fileName := strings.ToLower(methodList[0].FileBaseName()) + config.CppHandlerEx
	dstFileName := config.CentreMethodHandleDir + fileName
	data := getMethodHandlerCppStr(dstFileName, &methodList)
	util.WriteMd5Data2File(dstFileName, data)
}

func isCentreMethodRepliedHandler(methodList *RPCMethods) (check bool) {
	if len(*methodList) <= 0 {
		return false
	}
	firstMethodInfo := (*methodList)[0]
	if !(strings.Contains(firstMethodInfo.Path, config.ProtoDirNames[config.CommonProtoDirIndex]) ||
		strings.Contains(firstMethodInfo.Path, config.ProtoDirNames[config.LogicProtoDirIndex])) {
		return false
	}
	if !firstMethodInfo.CcGenericServices {
		return false
	}
	return !strings.Contains(firstMethodInfo.FileBaseName(), config.CentrePrefixName)
}

func writeCentreMethodRepliedHandlerHeadFile(methodList RPCMethods) {
	defer util.Wg.Done()

	if !isCentreMethodRepliedHandler(&methodList) {
		return
	}
	fileName := strings.ToLower(methodList[0].FileBaseName()) + config.HeadRepliedHandlerEx
	dstFileName := config.CentreMethodRepliedHandleDir + fileName
	data := getMethodRepliedHandlerHeadStr(&methodList)
	util.WriteMd5Data2File(dstFileName, data)
}

func writeCentreMethodRepliedHandlerCppFile(methodList RPCMethods) {
	defer util.Wg.Done()
	if !isCentreMethodRepliedHandler(&methodList) {
		return
	}
	firstMethodInfo := methodList[0]
	fileName := strings.ToLower(firstMethodInfo.FileBaseName()) + config.CppRepliedHandlerEx
	dstFileName := config.CentreMethodRepliedHandleDir + fileName
	data := getMethodRepliedHandlerCppStr(dstFileName, &methodList)
	util.WriteMd5Data2File(dstFileName, data)
}

func isCentrePlayerRepliedHandler(methodList *RPCMethods) (result bool) {
	if len(*methodList) <= 0 {
		return
	}
	firstMethodInfo := (*methodList)[0]
	if !firstMethodInfo.IsPlayerService() {
		return false
	}
	return !strings.Contains(firstMethodInfo.FileBaseName(), config.CentrePrefixName)
}

func writeCentrePlayerMethodRepliedHandlerHeadFile(methodList RPCMethods) {
	defer util.Wg.Done()
	if !isCentrePlayerRepliedHandler(&methodList) {
		return
	}
	fileName := methodList[0].FileBaseName() + config.HeadRepliedHandlerEx
	util.WriteMd5Data2File(config.CentreMethodRepliedHandleDir+fileName, getPlayerMethodRepliedHeadStr(methodList))
}

func writeCentrePlayerMethodRepliedHandlerCppFile(methodList RPCMethods) {
	defer util.Wg.Done()
	if !isCentrePlayerRepliedHandler(&methodList) {
		return
	}
	firstMethodInfo := methodList[0]
	fileName := strings.ToLower(methodList[0].FileBaseName()) + config.CppRepliedHandlerEx
	dstFileName := config.CentreMethodRepliedHandleDir + fileName
	data := getMethodPlayerHandlerCppStr(dstFileName,
		&methodList,
		firstMethodInfo.CppRepliedHandlerClassName(),
		firstMethodInfo.CppRepliedHandlerIncludeName())
	util.WriteMd5Data2File(dstFileName, data)
}

// /gate
func isGateMethodRepliedHandler(methodList *RPCMethods) (check bool) {
	if len(*methodList) <= 0 {
		return false
	}
	firstMethodInfo := (*methodList)[0]
	if !strings.Contains(firstMethodInfo.Path, config.ProtoDirNames[config.CommonProtoDirIndex]) {
		return false
	}
	if !firstMethodInfo.CcGenericServices {
		return false
	}
	return strings.Contains(firstMethodInfo.FileBaseName(), config.CentrePrefixName) ||
		strings.Contains(firstMethodInfo.FileBaseName(), config.DeployPrefixName) ||
		strings.Contains(firstMethodInfo.FileBaseName(), config.LobbyPrefixName) ||
		strings.Contains(firstMethodInfo.FileBaseName(), config.GsPrefixName) ||
		strings.Contains(firstMethodInfo.FileBaseName(), config.LoginPrefixName)
}

func isGateServiceHandler(methodList *RPCMethods) (check bool) {
	if len(*methodList) <= 0 {
		return false
	}
	firstMethodInfo := (*methodList)[0]
	if !strings.Contains(firstMethodInfo.Path, config.ProtoDirNames[config.CommonProtoDirIndex]) {
		return false
	}
	return strings.Contains(firstMethodInfo.FileBaseName(), "gate")
}

func writeGateMethodHandlerHeadFile(methodList RPCMethods) {
	defer util.Wg.Done()

	if len(methodList) <= 0 {
		return
	}
	if !isGateServiceHandler(&methodList) {
		return
	}
	fileName := methodList[0].FileBaseName() + config.HeadHandlerEx
	util.WriteMd5Data2File(config.GateMethodHandleDir+fileName, getServiceHandlerHeadStr(methodList))
}

func writeGateMethodHandlerCppFile(methodList RPCMethods) {
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
	util.WriteMd5Data2File(dstFileName, data)
}

func writeGateMethodRepliedHandlerHeadFile(methodList RPCMethods) {
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
	util.WriteMd5Data2File(dstFileName, data)
}

func writeGateMethodRepliedHandlerCppFile(methodList RPCMethods) {
	defer util.Wg.Done()
	if !isGateMethodRepliedHandler(&methodList) {
		return
	}
	firstMethodInfo := methodList[0]
	fileName := strings.ToLower(firstMethodInfo.FileBaseName()) + config.CppRepliedHandlerEx
	dstFileName := config.GateMethodRepliedHandleDir + fileName
	data := getMethodRepliedHandlerCppStr(dstFileName, &methodList)
	util.WriteMd5Data2File(dstFileName, data)
}

func WriteMethodFile() {
	for _, v := range ServiceMethodMap {
		util.Wg.Add(1)
		go writeServiceIdHeadFile(v)
		util.Wg.Add(1)
		go writeServiceIdCppFile(v)

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

		//centre
		util.Wg.Add(1)
		go writeCentreMethodHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeCentreMethodHandlerCppFile(v)
		util.Wg.Add(1)
		go writeCentrePlayerMethodHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeCentrePlayerMethodHandlerCppFile(v)
		util.Wg.Add(1)
		go writeCentreMethodRepliedHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeCentreMethodRepliedHandlerCppFile(v)
		util.Wg.Add(1)
		go writeCentrePlayerMethodRepliedHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeCentrePlayerMethodRepliedHandlerCppFile(v)

		//gate
		util.Wg.Add(1)
		go writeGateMethodHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeGateMethodHandlerCppFile(v)
		util.Wg.Add(1)
		go writeGateMethodRepliedHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeGateMethodRepliedHandlerCppFile(v)

	}
	//game
	util.Wg.Add(1)
	go writeRegisterFile(config.GsMethodHandleDir+config.RegisterHandlerCppEx, isGsMethodHandler)
	util.Wg.Add(1)
	go writeRepliedRegisterFile(config.GsMethodRepliedHandleDir+config.RegisterRepliedHandlerCppEx, isGsMethodRepliedHandler)

	//centre
	util.Wg.Add(1)
	go writeRegisterFile(config.CentreMethodHandleDir+config.RegisterHandlerCppEx, isCentreMethodHandler)
	util.Wg.Add(1)
	go writeRepliedRegisterFile(config.CentreMethodRepliedHandleDir+config.RegisterRepliedHandlerCppEx, isCentreMethodRepliedHandler)

	//gate
	util.Wg.Add(1)
	go writeRepliedRegisterFile(config.GateMethodRepliedHandleDir+config.RegisterRepliedHandlerCppEx, isGateMethodRepliedHandler)
}
