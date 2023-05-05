package gen

import (
	"bufio"
	"gengo/config"
	"gengo/util"
	"log"
	"os"
	"path/filepath"
	"reflect"
	"strconv"
	"strings"
)

const msg = "message"
const begin = "{"
const end = "}"
const setName = "::set_"
const mutableName = "::mutable_"
const mapType = "map"
const stringType = "string"

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
	var data = "#pragma once\n" + "#include <sol/sol.hpp>\n" +
		"#include \"src/game_logic/thread_local/thread_local_storage_lua.h\"\n"
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
	data += "std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> g_player_services;\n"
	data += "void InitPlayerService()\n{\n"
	data += instanceData
	data += "}"
	Md5WriteData2File(config.ClientServiceInstanceFile, data)
}

func toCppIntType(typeString string) (newType string, convert bool) {
	if reflect.DeepEqual(typeString, "int32") ||
		reflect.DeepEqual(typeString, "uint32") ||
		reflect.DeepEqual(typeString, "uint64") ||
		reflect.DeepEqual(typeString, "int64") {
		typeString = typeString + "_t"
		return typeString, true
	}
	return typeString, false
}

func GetTypeRef(typeString string) (valueTypeRef string) {
	if isCppType(typeString) {
		valueTypeRef, _ = toCppIntType(typeString)
	} else {
		valueTypeRef = typeString + "&"
	}
	return valueTypeRef
}

func isCppType(typeString string) bool {
	return reflect.DeepEqual(typeString, "int32") ||
		reflect.DeepEqual(typeString, "uint32") ||
		reflect.DeepEqual(typeString, "uint64") ||
		reflect.DeepEqual(typeString, "int64") ||
		reflect.DeepEqual(typeString, "bool") ||
		reflect.DeepEqual(typeString, "float") ||
		reflect.DeepEqual(typeString, "double")
}

func writeProtoSol2LuaFile(fd os.DirEntry, filePath string) {
	defer util.Wg.Done()
	isMsgCode := 0
	isEnumCode := 0
	fileBaseName := filepath.Base(strings.ToLower(strings.ReplaceAll(fd.Name(), config.ProtoEx, "")))

	f, err := os.Open(filePath + fd.Name())
	if err != nil {
		return
	}
	data := config.IncludeBegin + fileBaseName + config.ProtoPbhEx + config.IncludeEndLine +
		"#include <sol/sol.hpp>\n" +
		"#include \"src/game_logic/thread_local/thread_local_storage_lua.h\"\n" +
		"void Pb2sol2" + fileBaseName + "()" + "\n{\n"
	defer f.Close()
	scanner := bufio.NewScanner(f)
	className := ""
	for scanner.Scan() {
		line := scanner.Text()
		if strings.Contains(line, "enum") {
			isEnumCode = 1
			continue
		} else if strings.Contains(line, end) && isEnumCode == 1 {
			isEnumCode = 0
			continue
		} else if isEnumCode == 1 {
			continue
		}
		if strings.Contains(line, msg) {
			isMsgCode = 1
			className = strings.Trim(strings.Split(line, " ")[1], "\n")
			data += "tls_lua_state.new_usertype<" + className + ">(\"" + className + "\",\n"
			continue
		}
		if strings.Contains(line, begin) {
			continue
		} else if strings.Contains(line, end) && isMsgCode == 1 {
			data += "\"DebugString\",\n"
			data += "&" + className + "::DebugString,\n"
			data += "sol::base_classes, sol::bases<::google::protobuf::Message>());\n\n"
			isMsgCode = 0
			continue
		} else if isMsgCode == 1 {
			s := strings.Split(strings.Trim(line, "\t"), " ")
			if len(s) < 3 {
				continue
			}
			filedTypeName := s[0]
			filedName := s[1]
			templateName := ""
			isRepeatedFiled := true
			mapKeyType := ""
			mapValueType := ""
			//填入真正的类型
			if isCppType(filedTypeName) {
				templateName = ""
				isRepeatedFiled = false
			} else if filedTypeName == stringType {
				templateName = "<const std::string&>"
				isRepeatedFiled = false
			} else if filedTypeName == "repeated" {
				filedTypeName = s[1]
				filedName = s[2]
			} else if strings.Contains(filedTypeName, mapType) {
				filedTypeName = mapType
				mapKeyType = strings.Split(strings.Trim(s[0], "\tmap<"), ",")[0]
				mapKeyType, _ = toCppIntType(mapKeyType)
				mapValueType, _ = toCppIntType(strings.Split(strings.Trim(strings.Trim(s[1], "\t"), " "), ">")[0])
				filedName = s[2]
			} else {
				data += "\"" + filedName + "\",\n"
				data += "[](" + className + "& pb) ->decltype(auto){ return pb." + filedName + "();},\n"
				data += "\"mutable_" + filedName + "\",\n"
				data += "[](" + className + "& pb) ->decltype(auto){ return pb.mutable_" + filedName + "();},\n"
				continue
			}
			if !isRepeatedFiled {
				data += "\"" + filedName + "\",\n"
				data += "sol::property(&" + className + "::" + filedName + ", &" + className + setName + filedName
				data += templateName + "),\n"
			} else {
				if filedTypeName == "string" {
					templateName = "<const std::string&>"
					data += "\"add_" + filedName + "\",\n"
					data += "[](" + className + "& pb, const std::string& value) ->decltype(auto){ return pb.add_" +
						filedName + "(value);},\n"
					data += "\"" + filedName + "\",\n"
					data += "[](" + className + "& pb, int index) ->decltype(auto){ return pb." + filedName + "(index);},\n"
					data += "\"set_" + filedName + "\",\n"
					data += "[](" + className + "& pb, int index, const std::string& value) ->decltype(auto){ return pb.set_" +
						filedName + "(index, value);},\n"
				} else if filedTypeName == mapType {
					data += "\"count_" + filedName + "\",\n"
					data += "[](" + className + "& pb, " + GetTypeRef(mapKeyType) + " key) ->decltype(auto){ return pb." +
						filedName + "().count(key);},\n"
					data += "\"insert_" + filedName + "\",\n"
					data += "[](" + className + "& pb, " + GetTypeRef(mapKeyType) + " key, " + GetTypeRef(mapValueType) +
						" value) ->decltype(auto){ return pb.mutable_" + filedName + "()->emplace(key, value).second;},\n"
					data += "\"" + filedName + "\",\n"
					data += "[](" + className + "& pb, " + GetTypeRef(mapKeyType) + " key) ->decltype(auto){\n"
					data += " auto it =  pb.mutable_" + filedName + "()->find(key);\n"
					if isCppType(mapValueType) {
						vt, _ := toCppIntType(mapValueType)
						data += " if (it == pb.mutable_" + filedName + "()->end()){ return " + vt + "(); }\n"
					} else {
						data += " if (it == pb.mutable_" + filedName + "()->end()){ static " + mapValueType + " instance; return instance; }\n"
					}
					data += " return it->second;},\n"
				} else {
					typename, convert := toCppIntType(filedTypeName)
					if convert {
						data += "\"add_" + filedName + "\",\n"
						data += "&" + className + "::add_" + filedName + ",\n"
						data += "\"" + filedName + "\",\n"
						data += "[](" + className + "& pb, int index) ->decltype(auto){ return pb." + filedName + "(index);},\n"
						data += "\"set_" + filedName + "\",\n"
						data += "[](" + className + "& pb, int index, " + typename + " value) ->decltype(auto){ return pb.set_" + filedName + "(index, value);},\n"
					} else {
						data += "\"add_" + filedName + "\",\n"
						data += "&" + className + "::add_" + filedName + ",\n"
						data += "\"" + filedName + "\",\n"
						data += "[](" + className + "& pb, int index) ->decltype(auto){ return pb." + filedName + "(index);},\n"
						data += "\"mutable_" + filedName + "\",\n"
						data += "[](" + className + "& pb, int index) ->decltype(auto){ return pb.mutable_" + filedName + "(index);},\n"
					}
				}
				data += "\"" + filedName + "_size\",\n"
				data += "&" + className + "::" + filedName + "_size,\n"
				data += "\"clear_" + filedName + "\",\n"
				data += "&" + className + "::clear_" + filedName + ",\n"
			}
		}
	}
	data += "}\n"
	Md5WriteData2File(config.PbcSol2DirName+fileBaseName+config.CppSol2Ex, data)
}

func writeAllProtoSol2LuaFile() {
	for i := 0; i < len(config.ProtoDirs); i++ {
		fds, err := os.ReadDir(config.ProtoDirs[i])
		if err != nil {
			log.Fatal(err)
			continue
		}
		for _, fd := range fds {
			if !util.IsProtoFile(fd) {
				continue
			}
			util.Wg.Add(1)
			writeProtoSol2LuaFile(fd, config.ProtoDirs[i])
		}
	}
}

func writeLuaServiceFile() {
	for i := 0; i < len(config.ProtoDirs); i++ {
		//BuildProto(config.ProtoDirs[i], config.ProtoMd5Dirs[i])
	}
}

func WritePbcLua() {
	writeAllProtoSol2LuaFile()
}

func WriteClientServiceHeadHandlerFile() {
	for _, v := range ServiceMethodMap {
		util.Wg.Add(1)
		go writeClientMethodHandlerHeadFile(v)
	}
	util.Wg.Add(1)
	go writeClientHandlerDefaultInstanceFile()
}
