package gen

import (
	"fmt"
	"pbgen/config"
	"pbgen/util"
	"strconv"
	"strings"
)

// Type definitions for callback functions
type checkRepliedCb func(methodList *RPCMethods) bool

// Function to write the header file for service ID
func writeServiceIdHeadFile(methodList RPCMethods) {
	defer util.Wg.Done()

	if len(methodList) <= 0 {
		return
	}

	var data strings.Builder
	data.WriteString("#pragma once\n#include <cstdint>\n\n")
	data.WriteString(methodList[0].IncludeName() + "\n")

	for _, method := range methodList {
		data.WriteString(getServiceIdDefinitions(method))
		data.WriteString("\n")
	}

	fileName := methodList[0].FileBaseName() + config.ServiceInfoExtension + config.HeaderExtension
	util.WriteMd5Data2File(config.ServiceInfoDirectory+fileName, data.String())
}

// Helper function to generate service ID definitions
func getServiceIdDefinitions(method *RPCMethod) string {
	var data strings.Builder

	data.WriteString("constexpr uint32_t " + method.KeyName() + config.MessageIdName + " = " + strconv.FormatUint(method.Id, 10) + ";\n")
	data.WriteString("constexpr uint32_t " + method.KeyName() + "Index = " + strconv.FormatUint(method.Index, 10) + ";\n")

	data.WriteString("#define " + method.KeyName() + "Method  ::" + method.Service + "_Stub::descriptor()->method(" +
		strconv.FormatUint(method.Index, 10) + ")\n")

	return data.String()
}

// Function to get the header string for service handlers
func getServiceHandlerHeadStr(methodList RPCMethods) string {
	var data strings.Builder
	data.WriteString("#pragma once\n")
	data.WriteString(methodList[0].IncludeName())
	data.WriteString("class " + methodList[0].Service + "Handler : public ::" + methodList[0].Service + "\n{\npublic:\n")

	for _, method := range methodList {
		data.WriteString(getServiceHandlerMethodStr(method))
		data.WriteString("\n")
	}

	data.WriteString("};\n\n")
	return data.String()
}

// Helper function to generate method strings for service handlers
func getServiceHandlerMethodStr(method *RPCMethod) string {
	var data strings.Builder

	data.WriteString(config.Tab + "void " + method.Method + "(" + config.GoogleMethodController + "\n")
	data.WriteString(config.Tab2 + "const ::" + method.Request + "* request,\n")
	data.WriteString(config.Tab2 + "::" + method.Response + "* response,\n")
	data.WriteString(config.Tab2 + "::google::protobuf::Closure* done)override;\n\n")

	return data.String()
}

// Function to get the header string for player method handlers
func getPlayerMethodHeadStr(methodList RPCMethods) string {
	var data strings.Builder
	data.WriteString("#pragma once\n")
	data.WriteString(methodList[0].IncludeName())
	data.WriteString(config.PlayerServiceIncludeName)
	data.WriteString(config.MacroReturnIncludeName)
	data.WriteString("\nclass " + methodList[0].Service + config.HandlerFileName + " : public ::PlayerService" + "\n{\npublic:\n")
	data.WriteString(config.Tab + "using PlayerService::PlayerService;\n")

	data.WriteString(getPlayerMethodHandlerFunctions(methodList))
	data.WriteString("\n};\n")

	return data.String()
}

// Helper function to generate method handler functions for player methods
func getPlayerMethodHandlerFunctions(methodList RPCMethods) string {
	var data strings.Builder
	var callFunctionList strings.Builder

	for i, method := range methodList {
		data.WriteString(config.Tab + "static void " + method.Method + "(" + config.PlayerMethodController + "\n")
		data.WriteString(config.Tab2 + "const ::" + method.Request + "* request,\n")
		data.WriteString(config.Tab2 + "::" + method.Response + "* response);\n\n")

		callFunctionList.WriteString(config.Tab2 + "case " + strconv.Itoa(i) + ":\n")
		callFunctionList.WriteString(config.Tab3 + method.Method + "(player,\n")
		callFunctionList.WriteString(config.Tab3 + "static_cast<const " + method.Request + "*>(request),\n")
		callFunctionList.WriteString(config.Tab3 + "static_cast<" + method.Response + "*>(response));\n")

		if !strings.Contains(method.Response, config.EmptyResponseName) {
			callFunctionList.WriteString(config.Tab3 + "TRANSFER_ERROR_MESSAGE(static_cast<" + method.Response + "*>(response));\n")
		}

		callFunctionList.WriteString(config.Tab2 + "break;\n")
	}

	data.WriteString(config.Tab + "void CallMethod(const ::google::protobuf::MethodDescriptor* method,\n")
	data.WriteString(config.Tab2 + "entt::entity player,\n")
	data.WriteString(config.Tab2 + "const ::google::protobuf::Message* request,\n")
	data.WriteString(config.Tab2 + "::google::protobuf::Message* response)override \n")
	data.WriteString(config.Tab2 + "{\n")
	data.WriteString(config.Tab2 + "switch(method->index())\n")
	data.WriteString(config.Tab2 + "{\n")
	data.WriteString(callFunctionList.String())
	data.WriteString(config.Tab2 + "default:\n")
	data.WriteString(config.Tab2 + "break;\n")
	data.WriteString(config.Tab2 + "}\n")
	data.WriteString(config.Tab + "}\n")

	return data.String()
}

// Function to get the header string for player method replied handlers
func getPlayerMethodRepliedHeadStr(methodList RPCMethods) string {
	var data strings.Builder
	data.WriteString("#pragma once\n")
	data.WriteString(methodList[0].IncludeName())
	data.WriteString(config.PlayerServiceRepliedIncludeName)
	data.WriteString("\nclass " + methodList[0].Service + config.RepliedHandlerFileName + " : public ::PlayerServiceReplied" + "\n{\npublic:\n")
	data.WriteString(config.Tab + "using PlayerServiceReplied::PlayerServiceReplied;\n")

	data.WriteString(getPlayerMethodRepliedHandlerFunctions(methodList))
	data.WriteString("\n};\n")

	return data.String()
}

// Helper function to generate method replied handler functions for player methods
func getPlayerMethodRepliedHandlerFunctions(methodList RPCMethods) string {
	var data strings.Builder
	var callFunctionList strings.Builder

	for i, method := range methodList {
		data.WriteString(config.Tab + "static void " + method.Method + "(" + config.PlayerMethodController + "\n")
		data.WriteString(config.Tab2 + "const ::" + method.Request + "* request,\n")
		data.WriteString(config.Tab2 + "::" + method.Response + "* response);\n\n")

		callFunctionList.WriteString(config.Tab2 + "case " + strconv.Itoa(i) + ":\n")
		callFunctionList.WriteString(config.Tab3 + method.Method + "(player,\n")
		callFunctionList.WriteString(config.Tab3 + "nullptr,\n")
		callFunctionList.WriteString(config.Tab3 + "static_cast<" + method.Response + "*>(response));\n")
		callFunctionList.WriteString(config.Tab2 + "break;\n")
	}

	data.WriteString(config.Tab + "void CallMethod(const ::google::protobuf::MethodDescriptor* method,\n")
	data.WriteString(config.Tab2 + "entt::entity player,\n")
	data.WriteString(config.Tab2 + "const ::google::protobuf::Message* request,\n")
	data.WriteString(config.Tab2 + "::google::protobuf::Message* response)override \n")
	data.WriteString(config.Tab2 + "{\n")
	data.WriteString(config.Tab2 + "switch(method->index())\n")
	data.WriteString(config.Tab2 + "{\n")
	data.WriteString(callFunctionList.String())
	data.WriteString(config.Tab2 + "default:\n")
	data.WriteString(config.Tab2 + "break;\n")
	data.WriteString(config.Tab2 + "}\n")
	data.WriteString(config.Tab + "}\n")

	return data.String()
}

func getMethodRepliedHandlerHeadStr(methodList *RPCMethods) string {
	// Ensure there are methods in the list
	if len(*methodList) == 0 {
		return ""
	}

	var data strings.Builder
	firstMethodInfo := (*methodList)[0]

	// Start with pragma once and include the first method's specific include if available
	data.WriteString("#pragma once\n")
	data.WriteString(firstMethodInfo.IncludeName())
	data.WriteString("#include \"muduo/net/TcpConnection.h\"\n\n")
	data.WriteString("using namespace muduo;\n")
	data.WriteString("using namespace muduo::net;\n\n")

	// Generate handler function declarations for each method
	for _, methodInfo := range *methodList {
		handlerDeclaration := fmt.Sprintf("void On%s%s(const TcpConnectionPtr& conn, const std::shared_ptr<%s>& replied, Timestamp timestamp);\n\n",
			methodInfo.KeyName(), config.RepliedHandlerFileName, methodInfo.Response)
		data.WriteString(handlerDeclaration)
	}

	return data.String()
}

func getMethodHandlerCppStr(dst string, methodList *RPCMethods) string {
	// Ensure there are methods in the list
	if len(*methodList) == 0 {
		return ""
	}

	// Read code sections from file
	yourCodes, _ := util.ReadCodeSectionsFromFile(dst, len(*methodList)+1)

	var data strings.Builder
	firstMethodInfo := (*methodList)[0]

	// Start with the C++ handler include specific to the first method
	data.WriteString(firstMethodInfo.CppHandlerIncludeName())

	// Determine class name based on the first method's service and handler name configuration
	className := firstMethodInfo.Service + config.HandlerFileName

	// Iterate through yourCodes and methodList simultaneously
	for i, code := range yourCodes {
		// Calculate the corresponding method index in methodList
		methodIndex := i - 1

		if methodIndex >= 0 && methodIndex < len(*methodList) {
			// Retrieve method information
			methodInfo := (*methodList)[methodIndex]

			// Append method handler function definition
			data.WriteString(fmt.Sprintf("void %s::%s(%sconst ::%s* request,\n",
				className, methodInfo.Method, config.GoogleMethodController, methodInfo.Request))
			data.WriteString(config.Tab + "     " + methodInfo.Response + "* response,\n")
			data.WriteString(config.Tab + "     ::google::protobuf::Closure* done)\n")
			data.WriteString("{\n")
			data.WriteString(code) // Append existing code section
			data.WriteString("}\n\n")
		} else {
			// Append non-method related code directly
			data.WriteString(code)
		}
	}

	return data.String()
}

func getMethodRepliedHandlerCppStr(dst string, methodList *RPCMethods) string {
	// Ensure there are methods in the list
	if len(*methodList) == 0 {
		return ""
	}

	// Read code sections from file
	yourCodes, _ := util.ReadCodeSectionsFromFile(dst, len(*methodList)+1)

	var data strings.Builder
	firstMethodInfo := (*methodList)[0]

	// Start with the C++ replied handler include specific to the first method
	data.WriteString(firstMethodInfo.CppRepliedHandlerIncludeName())
	data.WriteString("#include \"network/codec/dispatcher.h\"\n\n")

	// External declaration for the dispatcher
	data.WriteString("extern ProtobufDispatcher gResponseDispatcher;\n\n")

	var declarationData, implData strings.Builder

	// Iterate through yourCodes and methodList simultaneously
	for i, code := range yourCodes {
		// Calculate the corresponding method index in methodList
		methodIndex := i - 1

		if methodIndex >= 0 && methodIndex < len(*methodList) {
			// Retrieve method information
			methodInfo := (*methodList)[methodIndex]

			// Construct function name for the handler
			funcName := "On" + methodInfo.KeyName() + config.RepliedHandlerFileName

			// Register message callback in declaration data
			declarationData.WriteString(fmt.Sprintf("%s%s", config.Tab,
				"gResponseDispatcher.registerMessageCallback<"+methodInfo.Response+
					">(std::bind(&"+funcName+", std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));\n"))

			// Implementation of the handler function
			implData.WriteString(fmt.Sprintf("void %s(const TcpConnectionPtr& conn, const std::shared_ptr<%s>& replied, Timestamp timestamp)\n{\n",
				funcName, methodInfo.Response))
			implData.WriteString(code) // Append existing code section
			implData.WriteString("}\n\n")
		} else {
			// Append non-method related code directly
			data.WriteString(code)
		}
	}

	// Initialize function for registering replied handler callbacks
	data.WriteString(fmt.Sprintf("\nvoid Init%s%s()\n{\n", firstMethodInfo.KeyName(), config.RepliedHandlerFileName))
	data.WriteString(declarationData.String())
	data.WriteString("}\n\n")

	// Append the implementation data for replied handler functions
	data.WriteString(implData.String())

	return data.String()
}

func getMethodPlayerHandlerCppStr(dst string, methodList *RPCMethods, className string, includeName string) string {
	// Ensure there are methods in the list
	if len(*methodList) == 0 {
		return includeName // Return the includeName if no methods are provided
	}

	// Read code sections from file
	yourCodes, _ := util.ReadCodeSectionsFromFile(dst, len(*methodList)+1)

	var data strings.Builder

	// Append the provided includeName
	data.WriteString(includeName)

	// Iterate through yourCodes and methodList simultaneously
	for i, code := range yourCodes {
		// Calculate the corresponding method index in methodList
		methodIndex := i - 1

		if methodIndex >= 0 && methodIndex < len(*methodList) {
			// Retrieve method information
			methodInfo := (*methodList)[methodIndex]

			// Append method handler function definition
			data.WriteString(fmt.Sprintf("void %s::%s(%sconst ::%s* request,\n",
				className, methodInfo.Method, config.PlayerMethodController, methodInfo.Request))
			data.WriteString(config.Tab + "     " + methodInfo.Response + "* response)\n")
			data.WriteString("{\n")
			data.WriteString(code) // Append existing code section
			data.WriteString("}\n\n")
		} else {
			// Append non-method related code directly
			data.WriteString(code)
		}
	}

	return data.String()
}

func writeRegisterFile(dst string, cb checkRepliedCb) {
	defer util.Wg.Done()

	var data strings.Builder
	var instanceData strings.Builder

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

		// Append C++ handler include specific to the first method
		data.WriteString(firstMethodInfo.CppHandlerIncludeName())

		// Append instance creation for the service handler
		instanceData.WriteString(fmt.Sprintf("%gNodeService.emplace(\"%s\", std::make_unique_for_overwrite<%s%s>());\n",
			config.Tab, firstMethodInfo.Service, firstMethodInfo.Service, config.HandlerFileName))
	}

	// Finalize the data string with the unordered_map declaration and initialization function
	data.WriteString("\nstd::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> gNodeService;\n\n")
	data.WriteString("void InitServiceHandler()\n{\n")
	data.WriteString(instanceData.String())
	data.WriteString("}")

	// Write the generated data to the destination file using util.WriteMd5Data2File
	util.WriteMd5Data2File(dst, data.String())
}

func writeRepliedRegisterFile(dst string, cb checkRepliedCb) {
	defer util.Wg.Done()

	var data strings.Builder

	// Start the function definition for initializing replied handlers
	data.WriteString("void InitRepliedHandler()\n{\n")

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

		// Append the initialization function declaration
		initFunctionName := "Init" + firstMethodInfo.KeyName() + config.RepliedHandlerFileName
		data.WriteString(config.Tab + "void " + initFunctionName + "();\n")

		// Call the initialization function
		data.WriteString(config.Tab + initFunctionName + "();\n\n")
	}

	// End the function definition
	data.WriteString("}\n")

	// Write the generated data to the destination file using util.WriteMd5Data2File
	util.WriteMd5Data2File(dst, data.String())
}

// / game server
func isGsMethodHandler(methodList *RPCMethods) bool {
	if len(*methodList) == 0 {
		return false
	}

	firstMethodInfo := (*methodList)[0]

	isCommonOrLogicProto := strings.Contains(firstMethodInfo.Path, config.ProtoDirectoryNames[config.CommonProtoDirIndex]) ||
		strings.Contains(firstMethodInfo.Path, config.ProtoDirectoryNames[config.LogicProtoDirIndex])

	if strings.Contains(firstMethodInfo.Path, config.PlayerName) ||
		strings.Contains(firstMethodInfo.FileBaseName(), config.PlayerName) {
		return false
	}

	hasGsPrefix := strings.HasPrefix(firstMethodInfo.FileBaseName(), config.GameNodePrefixName)

	return isCommonOrLogicProto && hasGsPrefix
}

func isGsPlayerHandler(methodList *RPCMethods) bool {
	if len(*methodList) <= 0 {
		return false
	}

	firstMethodInfo := (*methodList)[0]

	// Check if the method belongs to a player service
	if strings.Contains(firstMethodInfo.Path, config.ProtoDirectoryNames[config.ClientPlayerDirIndex]) {
		return true
	}

	// Check if the file base name contains player name and does not contain centre prefix
	fileBaseName := firstMethodInfo.FileBaseName()

	if !strings.Contains(fileBaseName, config.GameNodePlayerPrefixName) {
		return false
	}

	return true
}

func writeGsMethodHandlerHeadFile(methodList RPCMethods) {
	defer util.Wg.Done()

	// Check if the method list qualifies as a game server method handler
	if !isGsMethodHandler(&methodList) {
		return
	}

	// Generate the file name based on the first method's base name and configuration
	fileName := methodList[0].FileBaseName() + config.HandlerHeaderExtension

	// Write the generated data to the destination file using util.WriteMd5Data2File
	util.WriteMd5Data2File(config.GameNodeMethodHandlerDirectory+fileName, getServiceHandlerHeadStr(methodList))
}

func writeGsMethodHandlerCppFile(methodList RPCMethods) {
	defer util.Wg.Done()

	// Check if the method list qualifies as a game server method handler
	if !isGsMethodHandler(&methodList) {
		return
	}

	// Generate the file name based on the first method's base name and configuration
	fileName := strings.ToLower(methodList[0].FileBaseName()) + config.HandlerCppExtension
	dstFileName := config.GameNodeMethodHandlerDirectory + fileName

	// Generate the C++ handler file content
	data := getMethodHandlerCppStr(dstFileName, &methodList)

	// Write the generated data to the destination file using util.WriteMd5Data2File
	util.WriteMd5Data2File(dstFileName, data)
}

func writeGsPlayerMethodHandlerHeadFile(methodList RPCMethods) {
	defer util.Wg.Done()

	// Check if the method list qualifies as a game server player method handler
	if !isGsPlayerHandler(&methodList) {
		return
	}

	// Generate the file name based on the first method's base name and configuration
	fileName := methodList[0].FileBaseName() + config.HandlerHeaderExtension

	// Write the generated data to the destination file using util.WriteMd5Data2File
	util.WriteMd5Data2File(config.GameNodePlayerMethodHandlerDirectory+fileName, getPlayerMethodHeadStr(methodList))
}

func writeGsPlayerMethodHandlerCppFile(methodList RPCMethods) {
	defer util.Wg.Done()

	// Check if methodList is empty or does not qualify as a game server player method handler
	if len(methodList) <= 0 || !isGsPlayerHandler(&methodList) {
		return
	}

	firstMethodInfo := methodList[0]

	// Generate the file name based on the first method's base name and configuration
	fileName := strings.ToLower(firstMethodInfo.FileBaseName()) + config.HandlerCppExtension
	dstFileName := config.GameNodePlayerMethodHandlerDirectory + fileName

	// Generate the C++ handler file content
	data := getMethodPlayerHandlerCppStr(dstFileName,
		&methodList,
		firstMethodInfo.CppHandlerClassName(),
		firstMethodInfo.CppHandlerIncludeName())

	// Write the generated data to the destination file using util.WriteMd5Data2File
	util.WriteMd5Data2File(dstFileName, data)
}

func isGsPlayerRepliedHandler(methodList *RPCMethods) bool {
	if len(*methodList) <= 0 {
		return false
	}

	firstMethodInfo := (*methodList)[0]

	if strings.Contains(firstMethodInfo.Path, config.ProtoDirectoryNames[config.ClientPlayerDirIndex]) {
		return false
	}

	// Check if the file base name does not contain the GameNodePrefixName
	return strings.Contains(firstMethodInfo.FileBaseName(), config.CentrePlayerPrefixName)
}

func writeGsPlayerMethodRepliedHandlerHeadFile(methodList RPCMethods) {
	defer util.Wg.Done()

	// Check if the method list qualifies as a game server player replied handler
	if !isGsPlayerRepliedHandler(&methodList) {
		return
	}

	// Generate the file name based on the first method's base name and configuration
	fileName := methodList[0].FileBaseName() + config.RepliedHandlerHeaderExtension

	// Write the generated data to the destination file using util.WriteMd5Data2File
	util.WriteMd5Data2File(config.GameNodePlayerMethodRepliedHandlerDirectory+fileName, getPlayerMethodRepliedHeadStr(methodList))
}

func writeGsPlayerMethodRepliedHandlerCppFile(methodList RPCMethods) {
	defer util.Wg.Done()

	// Check if methodList is empty or does not qualify as a game server player replied handler
	if len(methodList) <= 0 || !isGsPlayerRepliedHandler(&methodList) {
		return
	}

	firstMethodInfo := methodList[0]

	// Generate the file name based on the first method's base name and configuration
	fileName := strings.ToLower(firstMethodInfo.FileBaseName()) + config.CppRepliedHandlerEx
	dstFileName := config.GameNodePlayerMethodRepliedHandlerDirectory + fileName

	// Generate the C++ replied handler file content
	data := getMethodPlayerHandlerCppStr(dstFileName,
		&methodList,
		firstMethodInfo.CppRepliedHandlerClassName(),
		firstMethodInfo.CppRepliedHandlerIncludeName())

	// Write the generated data to the destination file using util.WriteMd5Data2File
	util.WriteMd5Data2File(dstFileName, data)
}
func isGsMethodRepliedHandler(methodList *RPCMethods) bool {
	if len(*methodList) == 0 {
		return false
	}

	firstMethodInfo := (*methodList)[0]

	// Check if the method is from a valid path and has generic services enabled
	if !(strings.Contains(firstMethodInfo.Path, config.ProtoDirectoryNames[config.CommonProtoDirIndex]) ||
		strings.Contains(firstMethodInfo.Path, config.ProtoDirectoryNames[config.LogicProtoDirIndex])) {
		return false
	}

	if strings.Contains(firstMethodInfo.Path, config.ProtoDirectoryNames[config.ClientPlayerDirIndex]) {
		return false
	}

	if !firstMethodInfo.CcGenericServices {
		return false
	}

	// Check if the file base name contains specific keywords
	return strings.Contains(firstMethodInfo.FileBaseName(), config.CentrePrefixName) ||
		strings.Contains(firstMethodInfo.FileBaseName(), config.GatePrefixName)
}

func writeGsMethodRepliedHandlerHeadFile(methodList RPCMethods) {
	defer util.Wg.Done()

	if !isGsMethodRepliedHandler(&methodList) {
		return
	}

	fileBaseName := methodList[0].FileBaseName()
	fileName := strings.ToLower(fileBaseName) + config.RepliedHandlerHeaderExtension
	dstFileName := config.GameNodeMethodRepliedHandlerDirectory + fileName
	data := getMethodRepliedHandlerHeadStr(&methodList)
	util.WriteMd5Data2File(dstFileName, data)
}

func writeGsMethodRepliedHandlerCppFile(methodList RPCMethods) {
	defer util.Wg.Done()

	if !isGsMethodRepliedHandler(&methodList) {
		return
	}

	fileBaseName := methodList[0].FileBaseName()
	fileName := strings.ToLower(fileBaseName) + config.CppRepliedHandlerEx
	dstFileName := config.GameNodeMethodRepliedHandlerDirectory + fileName
	data := getMethodRepliedHandlerCppStr(dstFileName, &methodList)
	util.WriteMd5Data2File(dstFileName, data)
}

func isCentreMethodHandler(methodList *RPCMethods) bool {
	if len(*methodList) == 0 {
		return false
	}

	firstMethodInfo := (*methodList)[0]

	// 检查是否在 common 或 logic proto 目录中
	isInCommonOrLogicProtoDir := strings.Contains(firstMethodInfo.Path, config.ProtoDirectoryNames[config.CommonProtoDirIndex]) ||
		strings.Contains(firstMethodInfo.Path, config.ProtoDirectoryNames[config.LogicProtoDirIndex])

	// 如果不在 common 或 logic proto 目录中，直接返回 false
	if !isInCommonOrLogicProtoDir {
		return false
	}

	if strings.Contains(firstMethodInfo.Path, config.ProtoDirectoryNames[config.ClientPlayerDirIndex]) {
		return false
	}

	// 如果路径或文件名包含 PlayerName，直接返回 false
	if strings.Contains(firstMethodInfo.Path, config.PlayerName) ||
		strings.Contains(firstMethodInfo.FileBaseName(), config.PlayerName) {
		return false
	}

	// 检查文件名是否包含 Centre 前缀
	return strings.Contains(firstMethodInfo.FileBaseName(), config.CentrePrefixName)
}

func writeCentreMethodHandlerHeadFile(methodList RPCMethods) {
	defer util.Wg.Done()

	if !isCentreMethodHandler(&methodList) {
		return
	}

	firstMethodInfo := methodList[0]
	fileName := firstMethodInfo.FileBaseName() + config.HandlerHeaderExtension
	dstFileName := config.CentreNodeMethodHandlerDirectory + fileName

	data := getServiceHandlerHeadStr(methodList)
	util.WriteMd5Data2File(dstFileName, data)
}

func writeCentreMethodHandlerCppFile(methodList RPCMethods) {
	defer util.Wg.Done()

	if !isCentreMethodHandler(&methodList) {
		return
	}

	firstMethodInfo := methodList[0]
	fileBaseName := firstMethodInfo.FileBaseName()
	lowerFileName := strings.ToLower(fileBaseName) + config.HandlerCppExtension
	dstFileName := config.CentreNodeMethodHandlerDirectory + lowerFileName

	data := getMethodHandlerCppStr(dstFileName, &methodList)
	util.WriteMd5Data2File(dstFileName, data)
}

func isCentrePlayerHandler(methodList *RPCMethods) bool {
	if len(*methodList) == 0 {
		return false
	}

	firstMethodInfo := (*methodList)[0]

	// 检查文件名是否包含 Centre 前缀名
	return strings.Contains(firstMethodInfo.FileBaseName(), config.CentrePlayerPrefixName)
}

func writeCentrePlayerMethodHandlerHeadFile(methodList RPCMethods) {
	defer util.Wg.Done()

	if !isCentrePlayerHandler(&methodList) {
		return
	}

	firstMethodInfo := methodList[0]
	fileName := firstMethodInfo.FileBaseName() + config.HandlerHeaderExtension
	outputFilePath := config.CentreNodePlayerMethodHandlerDirectory + fileName

	data := getPlayerMethodHeadStr(methodList)
	util.WriteMd5Data2File(outputFilePath, data)
}

func writeCentrePlayerMethodHandlerCppFile(methodList RPCMethods) {
	defer util.Wg.Done()

	if !isCentrePlayerHandler(&methodList) {
		return
	}

	firstMethodInfo := methodList[0]
	fileName := strings.ToLower(firstMethodInfo.FileBaseName()) + config.HandlerCppExtension
	dstFileName := config.CentreNodePlayerMethodHandlerDirectory + fileName

	data := getMethodPlayerHandlerCppStr(
		dstFileName,
		&methodList,
		firstMethodInfo.CppHandlerClassName(),
		firstMethodInfo.CppHandlerIncludeName(),
	)

	util.WriteMd5Data2File(dstFileName, data)
}

func isCentreMethodRepliedHandler(methodList *RPCMethods) bool {
	if len(*methodList) == 0 {
		return false
	}

	firstMethodInfo := (*methodList)[0]

	if firstMethodInfo.IsPlayerService() {
		return false
	}

	// Ensure the file base name does not contain CentrePrefixName
	return strings.Contains(firstMethodInfo.FileBaseName(), config.GameNodePrefixName) ||
		strings.Contains(firstMethodInfo.FileBaseName(), config.GatePrefixName)
}

func writeCentreMethodRepliedHandlerHeadFile(methodList RPCMethods) {
	defer util.Wg.Done()

	if !isCentreMethodRepliedHandler(&methodList) {
		return
	}

	firstMethodInfo := methodList[0]
	fileName := strings.ToLower(firstMethodInfo.FileBaseName()) + config.RepliedHandlerHeaderExtension
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

func isCentrePlayerRepliedHandler(methodList *RPCMethods) bool {
	if len(*methodList) == 0 {
		return false
	}

	firstMethodInfo := (*methodList)[0]

	if strings.Contains(firstMethodInfo.Path, config.ProtoDirectoryNames[config.ClientPlayerDirIndex]) {
		return false
	}

	// Check if it's a player service and not containing CentrePrefixName in ProtoFileBaseName
	return firstMethodInfo.IsPlayerService() && !strings.Contains(firstMethodInfo.FileBaseName(), config.CentrePrefixName)
}

func writeCentrePlayerMethodRepliedHandlerHeadFile(methodList RPCMethods) {
	defer util.Wg.Done() // defer execution of util.Wg.Done() to mark completion

	// Check if the CentrePlayer has replied handler for the methodList
	if !isCentrePlayerRepliedHandler(&methodList) {
		return // If not, return early
	}

	// Construct the file name using the base name of the first method in methodList
	fileName := methodList[0].FileBaseName() + config.RepliedHandlerHeaderExtension

	// Write the MD5 data to a file located in CentreMethodRepliedHandleDir
	util.WriteMd5Data2File(config.CentrePlayerMethodRepliedHandlerDirectory+fileName, getPlayerMethodRepliedHeadStr(methodList))
}

func writeCentrePlayerMethodRepliedHandlerCppFile(methodList RPCMethods) {
	defer util.Wg.Done() // defer execution of util.Wg.Done() to mark completion

	// Check if the CentrePlayer has a replied handler for the methodList
	if !isCentrePlayerRepliedHandler(&methodList) {
		return // If not, return early
	}

	// Get the first method info from the methodList
	firstMethodInfo := methodList[0]

	// Construct the file name for the C++ handler file
	fileName := strings.ToLower(firstMethodInfo.FileBaseName()) + config.CppRepliedHandlerEx

	// Construct the destination file name including the directory path
	dstFileName := config.CentrePlayerMethodRepliedHandlerDirectory + fileName

	// Generate the C++ handler code as a string
	data := getMethodPlayerHandlerCppStr(
		dstFileName,
		&methodList,
		firstMethodInfo.CppRepliedHandlerClassName(),
		firstMethodInfo.CppRepliedHandlerIncludeName(),
	)

	// Write the generated C++ handler code to file
	util.WriteMd5Data2File(dstFileName, data)
}

// /gate
func isGateMethodRepliedHandler(methodList *RPCMethods) (check bool) {
	// Check if the methodList is empty
	if len(*methodList) <= 0 {
		return false
	}

	// Retrieve the first method information from methodList
	firstMethodInfo := (*methodList)[0]

	// Check if the method's path contains the common ProtoDirNames
	if !strings.Contains(firstMethodInfo.Path, config.ProtoDirectoryNames[config.CommonProtoDirIndex]) {
		return false
	}

	// Check if the method has CcGenericServices enabled
	if !firstMethodInfo.CcGenericServices {
		return false
	}

	// Check if the ProtoFileBaseName of the method contains any of the specified prefixes
	return strings.Contains(firstMethodInfo.FileBaseName(), config.CentrePrefixName) ||
		strings.Contains(firstMethodInfo.FileBaseName(), config.DeployPrefixName) ||
		strings.Contains(firstMethodInfo.FileBaseName(), config.GameNodePrefixName) ||
		strings.Contains(firstMethodInfo.FileBaseName(), config.LoginPrefixName)
}

func isGateServiceHandler(methodList *RPCMethods) (check bool) {
	// Check if the methodList is empty
	if len(*methodList) <= 0 {
		return false
	}

	// Retrieve the first method information from methodList
	firstMethodInfo := (*methodList)[0]

	// Check if the method's path contains the common ProtoDirNames
	if !strings.Contains(firstMethodInfo.Path, config.ProtoDirectoryNames[config.CommonProtoDirIndex]) {
		return false
	}

	// Check if the ProtoFileBaseName of the method contains the substring "gate"
	return strings.Contains(firstMethodInfo.FileBaseName(), "gate")
}

func writeGateMethodHandlerHeadFile(methodList RPCMethods) {
	defer util.Wg.Done()

	// Check if methodList is empty
	if len(methodList) <= 0 {
		return
	}

	// Check if methodList qualifies as a gate service handler
	if !isGateServiceHandler(&methodList) {
		return
	}

	// Construct the file name for the header handler file
	fileName := methodList[0].FileBaseName() + config.HandlerHeaderExtension

	// Write the MD5 data to the header handler file
	util.WriteMd5Data2File(config.GateMethodHandlerDirectory+fileName, getServiceHandlerHeadStr(methodList))
}

func writeGateMethodHandlerCppFile(methodList RPCMethods) {
	defer util.Wg.Done()

	// Check if methodList is empty
	if len(methodList) <= 0 {
		return
	}

	// Check if methodList qualifies as a gate service handler
	if !isGateServiceHandler(&methodList) {
		return
	}

	// Construct the file name for the C++ handler file
	fileName := strings.ToLower(methodList[0].FileBaseName()) + config.HandlerCppExtension
	dstFileName := config.GateMethodHandlerDirectory + fileName

	// Generate the C++ handler code as a string
	data := getMethodHandlerCppStr(dstFileName, &methodList)

	// Write the generated C++ handler code to file
	util.WriteMd5Data2File(dstFileName, data)
}

func writeGateMethodRepliedHandlerHeadFile(methodList RPCMethods) {
	defer util.Wg.Done()

	// Check if methodList is empty
	if len(methodList) <= 0 {
		return
	}

	// Check if methodList qualifies as a gate method replied handler
	if !isGateMethodRepliedHandler(&methodList) {
		return
	}

	// Construct the file name for the replied handler header file
	fileName := strings.ToLower(methodList[0].FileBaseName()) + config.RepliedHandlerHeaderExtension
	dstFileName := config.GateMethodRepliedHandlerDirectory + fileName

	// Generate the header handler code as a string
	data := getMethodRepliedHandlerHeadStr(&methodList)

	// Write the generated header handler code to file
	util.WriteMd5Data2File(dstFileName, data)
}

func writeGateMethodRepliedHandlerCppFile(methodList RPCMethods) {
	defer util.Wg.Done()

	// Check if methodList qualifies as a gate method replied handler
	if !isGateMethodRepliedHandler(&methodList) {
		return
	}

	// Retrieve the first method information from methodList
	firstMethodInfo := methodList[0]

	// Construct the file name for the C++ replied handler file
	fileName := strings.ToLower(firstMethodInfo.FileBaseName()) + config.CppRepliedHandlerEx
	dstFileName := config.GateMethodRepliedHandlerDirectory + fileName

	// Generate the C++ replied handler code as a string
	data := getMethodRepliedHandlerCppStr(dstFileName, &methodList)

	// Write the generated C++ replied handler code to file
	util.WriteMd5Data2File(dstFileName, data)
}

func WriteMethodFile() {
	for _, v := range ServiceMethodMap {
		// Start concurrent operations for each service method
		util.Wg.Add(1)
		go writeServiceIdHeadFile(v)

		// gs methods
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

		// centre methods
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

		// gate methods
		util.Wg.Add(1)
		go writeGateMethodHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeGateMethodHandlerCppFile(v)
		util.Wg.Add(1)
		go writeGateMethodRepliedHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeGateMethodRepliedHandlerCppFile(v)
	}

	// Concurrent operations for game, centre, and gate registers
	util.Wg.Add(1)
	go writeRegisterFile(config.GameNodeMethodHandlerDirectory+config.RegisterHandlerCppExtension, isGsMethodHandler)
	util.Wg.Add(1)
	go writeRepliedRegisterFile(config.GameNodeMethodRepliedHandlerDirectory+config.RegisterRepliedHandlerCppExtension, isGsMethodRepliedHandler)

	util.Wg.Add(1)
	go writeRegisterFile(config.CentreNodeMethodHandlerDirectory+config.RegisterHandlerCppExtension, isCentreMethodHandler)
	util.Wg.Add(1)
	go writeRepliedRegisterFile(config.CentreMethodRepliedHandleDir+config.RegisterRepliedHandlerCppExtension, isCentreMethodRepliedHandler)

	util.Wg.Add(1)
	go writeRepliedRegisterFile(config.GateMethodRepliedHandlerDirectory+config.RegisterRepliedHandlerCppExtension, isGateMethodRepliedHandler)
}
