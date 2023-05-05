package gen

import (
	"gengo/config"
	"gengo/util"
	"os"
	"strconv"
	"strings"
)

func WriteLoadClientLuaFile() {
	util.Wg.Add(1)
	go func() {
		defer util.Wg.Done()
		var fds []os.DirEntry
		var err error
		if fds, err = os.ReadDir(config.ClientLuaDir); err != nil {
			return
		}
		data := "#include <sol/sol.hpp>\n" +
			"#include \"muduo/base/Logging.h\"\n" +
			"#include \"src/util/file2string.h\"\n" +
			"#include \"src/game_logic/thread_local/thread_local_storage_lua.h\"\n" +
			"void InitServiceLua()\n{\n"
		data += config.Tab + "std::string contents;\n"
		for _, fd := range fds {
			if !util.IsLuaFile(fd) {
				continue
			}
			data += config.Tab + "contents = common::File2String(\"" + config.ClientLuaProjectRelative + fd.Name() + "\");\n"
			data += config.Tab + "{\n"
			data += config.Tab2 + "auto r = tls_lua_state.script(contents);\n"
			data += config.Tab2 + "if (!r.valid())\n"
			data += config.Tab2 + "{\n"
			data += config.Tab3 + "sol::error err = r;\n"
			data += config.Tab3 + "LOG_FATAL << err.what();\n"
			data += config.Tab2 + "}\n"
			data += config.Tab + "}\n"
		}
		data += "\n}\n"
		Md5WriteData2File(config.ClientLuaServiceFile, data)
	}()
}

func getClientMethodHandlerHeadStr(methodList RpcMethodInfos) string {
	var data = "#pragma once\n" + "#include <sol/sol.hpp>\n"
	data += methodList[0].IncludeName()
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
			config.Tab4 + "tls_lua_state[\"" + methodList[i].Method + "\"](\n" +
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

func writeClientMethodHandlerHeadFile(methodList RpcMethodInfos) {
	defer util.Wg.Done()
	if len(methodList) <= 0 {
		return
	}
	if !strings.Contains(methodList[0].Path, config.ProtoDirNames[config.ClientPlayerDirIndex]) {
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
	for _, v := range ServiceMethodMap {
		method1Info := v[0]
		if !strings.Contains(method1Info.Path, config.ProtoDirNames[config.ClientPlayerDirIndex]) {
			continue
		}
		includeData += config.IncludeBegin + method1Info.FileBaseName() + config.HeadHandlerEx + config.IncludeEndLine
		instanceData += config.Tab + "g_player_services.emplace(\"" + method1Info.Service +
			"\", std::make_unique<" + method1Info.Service + config.HandlerName + ">());\n"
	}
	data += includeData
	data += "std::unordered_map<std::string, std::unique_ptr<Service>> g_player_services;\n"
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
