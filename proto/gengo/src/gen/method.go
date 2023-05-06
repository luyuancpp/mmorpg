package gen

import (
	"gengo/config"
	"gengo/util"
	"strconv"
	"strings"
)

func writeCommonMethodHeadFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()

	if len(methodList) <= 0 {
		return
	}
	var data = "#pragma once\n#include <cstdint>\n\n"
	data += methodList[0].IncludeName() + "\n"
	for i := 0; i < len(methodList); i++ {
		data += "extern const uint32_t " + methodList[i].KeyName() + config.RpcIdName + ";\n"
		data += "extern const uint32_t " + methodList[i].KeyName() + "Index;\n"
		data += "#define " + methodList[i].KeyName() + "Method  ::" + methodList[i].Service + "_Stub::descriptor()->method(" +
			strconv.FormatUint(methodList[i].Index, 10) + ")\n"
		data += "\n"
	}
	fileName := methodList[0].FileBaseName() + "_service" + config.HeadEx
	Md5WriteData2File(config.PbcOutDir+fileName, data)
}

func writeCommonMethodCppFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()

	if len(methodList) <= 0 {
		return
	}
	var data = config.IncludeBegin + methodList[0].FileBaseName() + "_service" + config.HeadEx + config.IncludeEndLine

	for i := 0; i < len(methodList); i++ {
		data += "const uint32_t " + methodList[i].KeyName() + config.RpcIdName + " = " + strconv.FormatUint(methodList[i].Id, 10) + ";\n"
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

func writeGsMethodHandlerHeadFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()

	if len(methodList) <= 0 {
		return
	}
	if strings.Contains(methodList[0].FileBaseName(), config.PlayerName) {
		return
	}
	if !strings.Contains(methodList[0].Path, config.ProtoDirNames[config.LogicProtoDirIndex]) {
		if !strings.Contains(methodList[0].FileBaseName(), "game") {
			return
		}
	}
	fileName := methodList[0].FileBaseName() + config.HeadHandlerEx
	Md5WriteData2File(config.GsMethodHandleDir+fileName, getMethodHandlerHeadStr(methodList))
}

func writeControllerMethodHandlerHeadFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()

	if len(methodList) <= 0 {
		return
	}

	if strings.Contains(methodList[0].FileBaseName(), config.PlayerName) {
		return
	}
	if !strings.Contains(methodList[0].Path, config.ProtoDirNames[config.LogicProtoDirIndex]) {
		if !strings.Contains(methodList[0].FileBaseName(), "controller") {
			return
		}
	}
	fileName := methodList[0].FileBaseName() + config.HeadHandlerEx
	Md5WriteData2File(config.ControllerMethodHandleDir+fileName, getMethodHandlerHeadStr(methodList))
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
		functionNameList += config.Tab + "void " + methodList[i].Method + config.PlayerMethodController + "\n" +
			config.Tab2 + "const ::" + rq + "* request,\n" +
			config.Tab2 + "::" + rsp + "* response);\n\n"
		callFunctionList += config.Tab2 + "case " + strconv.Itoa(i) + ":\n"
		callFunctionList += config.Tab3 + methodList[i].Method + "(player,\n"
		callFunctionList += config.Tab3 + "::google::protobuf::internal::DownCast<const " + rq + "*>( request),\n"
		callFunctionList += config.Tab3 + "::google::protobuf::internal::DownCast<" + rsp + "*>(response));\n"
		callFunctionList += config.Tab2 + "break;\n"
	}
	callFunctionList += config.Tab2 + "default:\n" +
		config.Tab3 + "GOOGLE_LOG(FATAL) << \"Bad method index; this should never happen.\";\n" +
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
		functionNameList += config.Tab + "void " + methodList[i].Method + config.PlayerMethodController + "\n" +
			config.Tab2 + "const ::" + rq + "* request,\n" +
			config.Tab2 + "::" + rsp + "* response);\n\n"
		callFunctionList += config.Tab2 + "case " + strconv.Itoa(i) + ":\n"
		callFunctionList += config.Tab3 + methodList[i].Method + "(player,\n"
		callFunctionList += config.Tab3 + "nullptr,\n"
		callFunctionList += config.Tab3 + "::google::protobuf::internal::DownCast<" + rsp + "*>(response));\n"
		callFunctionList += config.Tab2 + "break;\n"
	}
	callFunctionList += config.Tab2 + "default:\n" +
		config.Tab3 + "GOOGLE_LOG(FATAL) << \"Bad method index; this should never happen.\";\n" +
		config.Tab2 + "break;\n" + config.Tab2 + "}\n" + config.Tab + "}\n"
	data += functionNameList
	data += callFunctionList
	data += "\n};\n"
	return data
}

func getMethodRepliedHandlerHeadStr(dst string, methodList *RpcMethodInfos) (data string) {
	methodLen := len(*methodList)
	firstMethodInfo := (*methodList)[0]
	data = firstMethodInfo.IncludeName() +
		"#include \"muduo/net/TcpConnection.h\"\n" +
		"using namespace muduo;\n" +
		"using namespace muduo::net;\n\n"

	for i := 0; i < methodLen; i++ {
		methodInfo := (*methodList)[i]
		if methodInfo.Response == config.GoogleEmptyProtoName {
			continue
		}
		data += "void On" + methodInfo.Method + config.RepliedHandlerName + "(const TcpConnectionPtr& conn, const " +
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
	data = firstMethodInfo.CppRepliedHandlerIncludeName()

	for i := 0; i < len(yourCodes); i++ {
		j := i - 1
		isMessage := j >= 0 && j < methodLen
		if isMessage {
			methodInfo := (*methodList)[j]
			if methodInfo.Response == config.GoogleEmptyProtoName {
				continue
			}
			data += "void On" + methodInfo.Method + config.RepliedHandlerName + "(const TcpConnectionPtr& conn, const " +
				"std::shared_ptr<" + methodInfo.Response + ">& replied, Timestamp timestamp)\n{\n"
		}
		data += yourCodes[i]
		if isMessage {
			data += "}\n\n"
		}
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

func writeMethodRepliedHandleCppFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
}

func writeGsPlayerMethodHandlerHeadFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()

	if len(methodList) <= 0 {
		return
	}
	if !strings.Contains(methodList[0].FileBaseName(), config.PlayerName) {
		return
	}
	fileName := methodList[0].FileBaseName() + config.HeadHandlerEx
	Md5WriteData2File(config.GsMethodHandleDir+fileName, getPlayerMethodHeadStr(methodList))
}

func writeGsPlayerMethodRepliedHandlerHeadFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()

	if len(methodList) <= 0 {
		return
	}
	if !strings.Contains(methodList[0].FileBaseName(), config.PlayerName) {
		return
	}
	fileName := methodList[0].FileBaseName() + config.HeadRepliedHandlerEx
	Md5WriteData2File(config.GsMethodRepliedHandleDir+fileName, getPlayerMethodRepliedHeadStr(methodList))
}

func writeGsPlayerMethodRepliedHandlerCppFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if len(methodList) <= 0 {
		return
	}
	firstMethodInfo := methodList[0]
	if !firstMethodInfo.IsPlayerService() {
		return
	}
	if !strings.Contains(firstMethodInfo.FileBaseName(), config.PlayerName) {
		return
	}
	fileName := strings.ToLower(methodList[0].FileBaseName()) + config.CppRepliedHandlerEx
	dstFileName := config.GsMethodRepliedHandleDir + fileName
	md5FileName := GetMd5FileName(dstFileName)
	data := getMethodPlayerHandlerCppStr(dstFileName,
		&methodList,
		firstMethodInfo.CppRepliedHandlerClassName(),
		firstMethodInfo.CppRepliedHandlerIncludeName())
	Md5WriteData2File(md5FileName, data)
	Md5Copy(dstFileName, md5FileName)
}

func writeControllerPlayerMethodRepliedHandlerCppFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if len(methodList) <= 0 {
		return
	}
	firstMethodInfo := methodList[0]
	if !firstMethodInfo.IsPlayerService() {
		return
	}
	fileName := strings.ToLower(methodList[0].FileBaseName()) + config.CppRepliedHandlerEx
	dstFileName := config.ControllerMethodRepliedHandleDir + fileName
	md5FileName := GetMd5FileName(dstFileName)
	data := getMethodPlayerHandlerCppStr(dstFileName,
		&methodList,
		firstMethodInfo.CppRepliedHandlerClassName(),
		firstMethodInfo.CppRepliedHandlerIncludeName())
	Md5WriteData2File(md5FileName, data)
	Md5Copy(dstFileName, md5FileName)
}

func writeControllerPlayerMethodRepliedHandlerHeadFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()

	if len(methodList) <= 0 {
		return
	}
	if !strings.Contains(methodList[0].FileBaseName(), config.PlayerName) {
		return
	}
	fileName := methodList[0].FileBaseName() + config.HeadRepliedHandlerEx
	Md5WriteData2File(config.ControllerMethodRepliedHandleDir+fileName, getPlayerMethodRepliedHeadStr(methodList))
}

func writeGsMethodHandlerCppFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if len(methodList) <= 0 {
		return
	}
	if strings.Contains(methodList[0].Path, config.ProtoDirNames[config.CommonProtoDirIndex]) {
		if !strings.Contains(methodList[0].FileBaseName(), "game") {
			return
		}
	} else if !strings.Contains(methodList[0].Path, config.ProtoDirNames[config.LogicProtoDirIndex]) {
		return
	}
	fileName := strings.ToLower(methodList[0].FileBaseName()) + config.CppHandlerEx
	dstFileName := config.GsMethodHandleDir + fileName
	md5FileName := GetMd5FileName(dstFileName)
	data := getMethodHandlerCppStr(dstFileName, &methodList)
	Md5WriteData2File(md5FileName, data)
	Md5Copy(dstFileName, md5FileName)
}

func writeGsPlayerMethodHandlerCppFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if len(methodList) <= 0 {
		return
	}
	firstMethodInfo := methodList[0]
	if !firstMethodInfo.IsPlayerService() {
		return
	}
	fileName := strings.ToLower(methodList[0].FileBaseName()) + config.CppHandlerEx
	dstFileName := config.GsMethodHandleDir + fileName
	md5FileName := GetMd5FileName(dstFileName)
	data := getMethodPlayerHandlerCppStr(dstFileName,
		&methodList,
		firstMethodInfo.CppHandlerClassName(),
		firstMethodInfo.CppHandlerIncludeName())
	Md5WriteData2File(md5FileName, data)
	Md5Copy(dstFileName, md5FileName)
}

func writeControllerPlayerMethodHandlerCppFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if len(methodList) <= 0 {
		return
	}
	firstMethodInfo := methodList[0]
	if !firstMethodInfo.IsPlayerService() {
		return
	}
	fileName := strings.ToLower(methodList[0].FileBaseName()) + config.CppHandlerEx
	dstFileName := config.ControllerMethodHandleDir + fileName
	md5FileName := GetMd5FileName(dstFileName)
	data := getMethodPlayerHandlerCppStr(dstFileName,
		&methodList,
		firstMethodInfo.CppHandlerClassName(),
		firstMethodInfo.CppHandlerIncludeName())
	Md5WriteData2File(md5FileName, data)
	Md5Copy(dstFileName, md5FileName)
}

func writeControllerPlayerMethodHandlerHeadFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()

	if len(methodList) <= 0 {
		return
	}
	if !methodList[0].IsPlayerService() {
		return
	}
	fileName := methodList[0].FileBaseName() + config.HeadHandlerEx
	Md5WriteData2File(config.ControllerMethodHandleDir+fileName, getPlayerMethodHeadStr(methodList))
}

func writeControllerMethodHandlerCppFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if len(methodList) <= 0 {
		return
	}

	if strings.Contains(methodList[0].Path, config.ProtoDirNames[config.CommonProtoDirIndex]) {
		if !strings.Contains(methodList[0].FileBaseName(), "controller") {
			return
		}
	} else if !strings.Contains(methodList[0].Path, config.ProtoDirNames[config.LogicProtoDirIndex]) {
		return
	}

	fileName := strings.ToLower(methodList[0].FileBaseName()) + config.CppHandlerEx
	dstFileName := config.ControllerMethodHandleDir + fileName
	md5FileName := GetMd5FileName(dstFileName)
	data := getMethodHandlerCppStr(dstFileName, &methodList)
	Md5WriteData2File(md5FileName, data)
	Md5Copy(dstFileName, md5FileName)
}

func writeGsMethodRepliedHandlerHeadFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if len(methodList) <= 0 {
		return
	}
	path := methodList[0].Path
	fileBaseName := methodList[0].FileBaseName()
	if strings.Contains(path, config.ProtoDirNames[config.CommonProtoDirIndex]) {
		if !strings.Contains(fileBaseName, "controller") {
			return
		}
	} else if !strings.Contains(path, config.ProtoDirNames[config.LogicProtoDirIndex]) {
		return
	}

	fileName := strings.ToLower(fileBaseName) + config.HeadRepliedHandlerEx
	dstFileName := config.GsMethodRepliedHandleDir + fileName
	data := getMethodRepliedHandlerHeadStr(dstFileName, &methodList)
	Md5WriteData2File(dstFileName, data)
}

func writeGsMethodRepliedHandlerCppFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if len(methodList) <= 0 {
		return
	}

	path := methodList[0].Path
	fileBaseName := methodList[0].FileBaseName()
	if strings.Contains(path, config.ProtoDirNames[config.CommonProtoDirIndex]) {
		if !strings.Contains(fileBaseName, "controller") {
			return
		}
	} else if !strings.Contains(path, config.ProtoDirNames[config.LogicProtoDirIndex]) {
		return
	}

	fileName := strings.ToLower(fileBaseName) + config.CppRepliedHandlerEx
	dstFileName := config.GsMethodRepliedHandleDir + fileName
	data := getMethodRepliedHandlerCppStr(dstFileName, &methodList)
	Md5WriteData2File(dstFileName, data)
}

func writeControllerMethodRepliedHandlerHeadFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if len(methodList) <= 0 {
		return
	}

	if !(strings.Contains(methodList[0].Path, config.ProtoDirNames[config.CommonProtoDirIndex]) ||
		strings.Contains(methodList[0].Path, config.ProtoDirNames[config.LogicProtoDirIndex])) {
		return
	}
	fileName := strings.ToLower(methodList[0].FileBaseName()) + config.HeadRepliedHandlerEx
	dstFileName := config.ControllerMethodRepliedHandleDir + fileName
	data := getMethodRepliedHandlerHeadStr(dstFileName, &methodList)
	Md5WriteData2File(dstFileName, data)
}

func writeControllerMethodRepliedHandlerCppFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if len(methodList) <= 0 {
		return
	}

	if !(strings.Contains(methodList[0].Path, config.ProtoDirNames[config.CommonProtoDirIndex]) ||
		strings.Contains(methodList[0].Path, config.ProtoDirNames[config.LogicProtoDirIndex])) {
		return
	}

	fileName := strings.ToLower(methodList[0].FileBaseName()) + config.CppRepliedHandlerEx
	dstFileName := config.ControllerMethodRepliedHandleDir + fileName
	data := getMethodRepliedHandlerCppStr(dstFileName, &methodList)
	Md5WriteData2File(dstFileName, data)
}

func WriteMethodFile() {
	for _, v := range ServiceMethodMap {
		util.Wg.Add(1)
		go writeCommonMethodHeadFile(v)
		util.Wg.Add(1)
		go writeCommonMethodCppFile(v)

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

		util.Wg.Add(1)
		go writeMethodRepliedHandleCppFile(v)

	}
}
