package gen

import (
	"bufio"
	"fmt"
	"gengo/config"
	"gengo/util"
	"log"
	"os"
	"strconv"
	"strings"
)

func writeMethodHeadFile(methodList RpcMethodInfos) {
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
			strconv.FormatUint(methodList[i].Index, 10) + ");\n"
		data += "\n"
	}
	fileName := methodList[0].FileBaseName() + "_service" + config.HeadEx
	Md5WriteData2File(config.PbcOutDir+fileName, data)
}

func writeMethodCppFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()

	if len(methodList) <= 0 {
		return
	}
	var data = config.ProtoPbhIncludeBegin + methodList[0].FileBaseName() + "_service" + config.HeadEx + config.IncludeEndLine

	for i := 0; i < len(methodList); i++ {
		data += "const uint32_t " + methodList[i].KeyName() + config.RpcIdName + " = " + strconv.FormatUint(methodList[i].Id, 10) + ";\n"
		data += "const uint32_t " + methodList[i].KeyName() + "Index = " + strconv.FormatUint(methodList[i].Index, 10) + ";\n"
	}
	fileName := methodList[0].FileBaseName() + "_service" + config.CppEx
	Md5WriteData2File(config.PbcOutDir+fileName, data)
}

func getMethodHandlerHeadStr(methodList RpcMethodInfos) string {

	var data = "#pragma once\n"
	data += config.ProtoPbhIncludeBegin + methodList[0].FileBaseName() + config.ProtoPbhIncludeEndLine
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
	fileName := methodList[0].FileBaseName() + "_handler" + config.HeadEx
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
	fileName := methodList[0].FileBaseName() + "_handler" + config.HeadEx
	Md5WriteData2File(config.ControllerMethodHandleDir+fileName, getMethodHandlerHeadStr(methodList))
}

func getPlayerMethodHeadStr(methodList RpcMethodInfos) string {

	var data = "#pragma once\n"
	data += config.ProtoPbhIncludeBegin + methodList[0].FileBaseName() + config.ProtoPbhIncludeEndLine
	data += config.PlayerServiceIncludeName
	data += "\nclass " + methodList[0].Service + config.HandlerName + " : public ::PlayerService" + "\n{\npublic:\n"
	data += config.Tab + "PlayerService::PlayerService;\n"
	var functionNameList string
	var callFunctionList = " void CallMethod(const ::google::protobuf::MethodDescriptor* method,\n   " +
		"entt::entity player,\n    " +
		"const ::google::protobuf::Message* request,\n    " +
		"::google::protobuf::Message* response)override \n " +
		" {\n        switch(method->index()) {\n"

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
	data += "};\n\n"
	return data
}

func getMethodHandlerCppStr(methodList RpcMethodInfos, dst string) string {

	var data = config.ProtoPbhIncludeBegin + methodList[0].FileBaseName() + config.ProtoPbhIncludeEndLine +
		"#include \"src/game_logic/thread_local/thread_local_storage.h\"\n" +
		"#include \"src/network/message_system.h\"" +
		"// 大家注意把逻辑写的简洁一点，防止文件过大导致编译过久，和生成文件工具读取文件内存不够\n"

	f, err := os.Open(dst)
	if err != nil {
		log.Fatal(err)
		return ""
	}
	defer f.Close()

	scanner := bufio.NewScanner(f)
	var line string
	yourCodeIndex := 0
	var yourCodes []string
	for scanner.Scan() {
		line = scanner.Text()
		if strings.Contains(line, config.YourCodeBegin) {
			yourCodes = append(yourCodes, line)
		} else if strings.Contains(line, config.YourCodeEnd) {
			yourCodes = append(yourCodes, line)
			yourCodeIndex += 1
		} else if yourCodeIndex < len(yourCodes) {
			yourCodes[yourCodeIndex] += line
		}
	}

	for i := 0; i < len(yourCodes); i++ {
		j := i - 1
		isMethodIndex := j >= 0 && j < len(methodList)
		if isMethodIndex {
			data += "void " + methodList[j].Method + config.GoogleMethodController + "\n" +
				config.Tab + "const ::" + methodList[j].Request + "* request,\n" +
				config.Tab + "::" + methodList[j].Response + "* response,\n" +
				config.Tab + " ::google::protobuf::Closure* done)override;\n{\n"
		}
		data += yourCodes[i]

		if isMethodIndex {
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
	fileName := methodList[0].FileBaseName() + "_handler" + config.HeadEx
	Md5WriteData2File(config.GsMethodHandleDir+fileName, getPlayerMethodHeadStr(methodList))
}

func writeGsMethodHandlerCppFile(methodList RpcMethodInfos) {
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

	fileName := methodList[0].FileBaseName() + "_handler" + config.CppEx
	fmt.Println(fileName)
	//fmt.Println(getMethodHandlerCppStr(methodList, config.GsMethodHandleDir+fileName))
}

func writeControllerPlayerMethodHandlerHeadFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()

	if len(methodList) <= 0 {
		return
	}
	if !strings.Contains(methodList[0].FileBaseName(), config.PlayerName) {
		return
	}
	if !strings.Contains(methodList[0].FileBaseName(), config.ClientPlayerName) {
		return
	}
	fileName := methodList[0].FileBaseName() + "_handler" + config.HeadEx
	Md5WriteData2File(config.ControllerMethodHandleDir+fileName, getPlayerMethodHeadStr(methodList))
}

func writePlayerMethodHandlerCppFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()

	if len(methodList) <= 0 {
		return
	}
}

func writePlayerMethodRepliedHandleCppFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()

	if len(methodList) <= 0 {
		return
	}
}

func WriteMethodFile() {
	for _, v := range ServiceMethodMap {
		util.Wg.Add(1)
		go writeMethodHeadFile(v)
		util.Wg.Add(1)
		go writeMethodCppFile(v)
		util.Wg.Add(1)
		go writeGsMethodHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeControllerMethodHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeGsMethodHandlerCppFile(v)
		util.Wg.Add(1)
		go writeMethodRepliedHandleCppFile(v)
		util.Wg.Add(1)
		go writeGsPlayerMethodHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeControllerPlayerMethodHandlerHeadFile(v)
		util.Wg.Add(1)
		go writePlayerMethodHandlerCppFile(v)
		util.Wg.Add(1)
		go writePlayerMethodRepliedHandleCppFile(v)
	}
}
