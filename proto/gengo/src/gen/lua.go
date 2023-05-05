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
	var data = "#pragma once\n" + config.IncludeBegin + "<sol/sol.hpp>" + config.ProtoPbhIncludeEndLine
	data += config.IncludeBegin + methodList[0].FileBaseName() + config.ProtoPbhIncludeEndLine
	data += "class " + methodList[0].Service + "Handler : public ::" + methodList[0].Service + "\n{\npublic:\n"
	data += "void CallMethod(const ::google::protobuf::MethodDescriptor* method,\n" +
		"::google::protobuf::RpcController* controller,\n" +
		"const ::google::protobuf::Message* request,\n" +
		"::google::protobuf::Message* response,\n" +
		"::google::protobuf::Closure* done)override\n" +
		config.Tab + "{\n" + config.Tab2 + " switch(method->index())\n" +
		config.Tab2 + "{\n"
	for i := 0; i < len(methodList); i++ {
		data += config.Tab3 + "case " + strconv.Itoa(i) + ":\n" + config.Tab2 + "{\n" +
			config.Tab3 + "tls_lua_state[\"" + methodList[i].Method + "\"](\n" +
			config.Tab3 + "::google::protobuf::internal::DownCast<const ::" + methodList[i].Request + "*>( request),\n" +
			config.Tab3 + "::google::protobuf::internal::DownCast<::" + methodList[i].Response + "*>(response));\n" +
			config.Tab2 + "}\n" + config.Tab2 + "break;\n"
	}
	data += config.Tab2 + "default:\n" +
		config.Tab3 + "GOOGLE_LOG(FATAL) << \"Bad method index; this should never happen.\";" +
		config.Tab2 + "break;\n" + config.Tab2 + "};\n" + config.Tab + "};\n" + "};\n"
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

func WriteClientServiceHeadHandlerFile() {
	for _, v := range ServiceMethodMap {
		util.Wg.Add(1)
		go writeClientMethodHandlerHeadFile(v)
	}
}
