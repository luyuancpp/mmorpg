package gen

import (
	"bufio"
	"fmt"
	"os"
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

	firstMethod := methodList[0]

	if !firstMethod.CcGenericServices() {
		return
	}

	var data strings.Builder
	data.WriteString("#pragma once\n#include <cstdint>\n\n")
	data.WriteString(methodList[0].IncludeName() + "\n")

	for _, method := range methodList {
		data.WriteString(getServiceIdDefinitions(method))
		data.WriteString("\n")
	}

	fileName := methodList[0].FileNameNoEx() + config.ServiceInfoExtension + config.HeaderExtension
	util.WriteMd5Data2File(config.ServiceInfoDirectory+fileName, data.String())
}

// Helper function to generate service ID definitions
func getServiceIdDefinitions(method *RPCMethod) string {
	var data strings.Builder

	data.WriteString("constexpr uint32_t " + method.KeyName() + config.MessageIdName + " = " + strconv.FormatUint(method.Id, 10) + ";\n")
	data.WriteString("constexpr uint32_t " + method.KeyName() + "Index = " + strconv.FormatUint(method.Index, 10) + ";\n")

	data.WriteString("#define " + method.KeyName() + "Method  ::" + method.Service() + "_Stub::descriptor()->method(" +
		strconv.FormatUint(method.Index, 10) + ")\n")

	return data.String()
}

// Function to get the header string for service handlers
func getServiceHandlerHeadStr(methodList RPCMethods) string {
	var data strings.Builder
	data.WriteString("#pragma once\n")
	data.WriteString(methodList[0].IncludeName())
	data.WriteString("class " + methodList[0].Service() + "Handler : public ::" + methodList[0].Service() + "\n{\npublic:\n")

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

	data.WriteString(config.Tab + "void " + method.Method() + "(" + config.GoogleMethodController + "\n")
	data.WriteString(config.Tab2 + "const " + method.CppRequest() + "* request,\n")
	data.WriteString(config.Tab2 + method.CppResponse() + "* response,\n")
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
	data.WriteString("\nclass " + methodList[0].Service() + config.HandlerFileName + " : public ::PlayerService" + "\n{\npublic:\n")
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
		data.WriteString(config.Tab + "static void " + method.Method() + "(" + config.PlayerMethodController + "\n")
		data.WriteString(config.Tab2 + "const " + method.CppRequest() + "* request,\n")
		data.WriteString(config.Tab2 + method.CppResponse() + "* response);\n\n")

		callFunctionList.WriteString(config.Tab2 + "case " + strconv.Itoa(i) + ":\n")
		callFunctionList.WriteString(config.Tab3 + method.Method() + "(player,\n")
		callFunctionList.WriteString(config.Tab3 + "static_cast<const " + method.CppRequest() + "*>(request),\n")
		callFunctionList.WriteString(config.Tab3 + "static_cast<" + method.CppResponse() + "*>(response));\n")

		if !strings.Contains(method.CppResponse(), config.EmptyResponseName) {
			callFunctionList.WriteString(config.Tab3 + "TRANSFER_ERROR_MESSAGE(static_cast<" + method.CppResponse() + "*>(response));\n")
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
	data.WriteString("\nclass " + methodList[0].Service() + config.RepliedHandlerFileName + " : public ::PlayerServiceReplied" + "\n{\npublic:\n")
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
		data.WriteString(config.Tab + "static void " + method.Method() + "(" + config.PlayerMethodController + "\n")
		data.WriteString(config.Tab2 + "const " + method.CppRequest() + "* request,\n")
		data.WriteString(config.Tab2 + method.CppResponse() + "* response);\n\n")

		callFunctionList.WriteString(config.Tab2 + "case " + strconv.Itoa(i) + ":\n")
		callFunctionList.WriteString(config.Tab3 + method.Method() + "(player,\n")
		callFunctionList.WriteString(config.Tab3 + "nullptr,\n")
		callFunctionList.WriteString(config.Tab3 + "static_cast<" + method.CppResponse() + "*>(response));\n")
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
			methodInfo.KeyName(), config.RepliedHandlerFileName, methodInfo.CppResponse())
		data.WriteString(handlerDeclaration)
	}

	return data.String()
}

// ReadCodeSectionsFromFile 函数接收一个函数作为参数，动态选择 A 或 B 方法
func ReadCodeSectionsFromFile(cppFileName string, methodList *RPCMethods, methodFunc func(info *RPCMethod) string) (map[string]string, string, error) {
	// 创建一个 map 来存储每个 RPCMethod 的 name 和对应的 yourCode
	codeMap := make(map[string]string)

	fd, err := os.Open(cppFileName)
	if err != nil {
		return nil, "", fmt.Errorf("failed to open file %s: %v", cppFileName, err)
	}
	defer fd.Close()

	// 创建一个扫描器来按行读取文件
	scanner := bufio.NewScanner(fd)

	// 记录当前正在处理的 yourCode
	var currentCode string
	var currentMethod *RPCMethod
	var firstCode string // 用于保存第一个特殊的 yourCode
	var isFirstCode bool // 标记是否处理了第一个特殊的 yourCode
	var inFirstCode bool // 标记是否在处理第一个特殊的 yourCode
	var inMethodCode bool

	// 遍历文件的每一行
	for scanner.Scan() {
		line := scanner.Text() + "\n"

		// 如果正在处理第一个 yourCode，并且发现 YourCodeEnd
		if inFirstCode && strings.Contains(line, config.YourCodeEnd) {
			firstCode += line
			inFirstCode = false
			continue // 跳过其他处理，继续后续的代码处理
		} else if inFirstCode {
			firstCode += line
		}

		// 如果是第一个特殊的 yourCode块
		if !isFirstCode && strings.Contains(line, config.YourCodeBegin) {
			firstCode = line
			inFirstCode = true
			isFirstCode = true
			continue
		}

		if nil == currentMethod {
			// 如果是方法的开始行，检查是否是我们关心的 RPCMethod 名称
			for _, method := range *methodList {
				handlerName := methodFunc(method)
				if strings.Contains(line, handlerName) {
					currentMethod = method
					break
				}
			}
		}

		// 如果找到了当前方法的开始，接着读取直到找到结束
		if currentMethod != nil {
			if strings.Contains(line, config.YourCodeBegin) {
				inMethodCode = true
				currentCode += line
			} else if strings.Contains(line, config.YourCodeEnd) {
				currentCode += line
				// 使用 methodFunc currentMethod
				handlerName := methodFunc(currentMethod)
				codeMap[handlerName] = currentCode
				currentMethod = nil
				currentCode = ""
				inMethodCode = false
			} else if inMethodCode {
				currentCode += line
			}
		}
	}

	// 如果没有找到第一个 yourCode，使用默认的 config.YourCodePair
	if firstCode == "" {
		firstCode = config.YourCodePair
	}

	// 检查是否有方法没有找到对应的 yourCode，如果没有找到，则添加默认值
	for _, method := range *methodList {
		handlerName := methodFunc(method)
		if _, exists := codeMap[handlerName]; !exists {
			codeMap[handlerName] = config.YourCodePair
		}
	}

	return codeMap, firstCode, nil
}

func GenerateMethodHandlerNameWrapper(info *RPCMethod) string {
	return info.Service() + config.HandlerFileName + "::" + info.Method()
}

func GenerateMethodHandlerKeyNameWrapper(info *RPCMethod) string {
	return info.KeyName() // 调用方法并返回结果
}

func getMethodHandlerCppStr(dst string, methodList *RPCMethods) string {
	// 确保方法列表非空
	if len(*methodList) == 0 {
		return ""
	}

	// 读取代码块
	yourCodesMap, firstCode, _ := ReadCodeSectionsFromFile(dst, methodList, GenerateMethodHandlerNameWrapper)

	var data strings.Builder
	firstMethodInfo := (*methodList)[0]

	// 写入 C++ 处理函数的头部
	data.WriteString(firstMethodInfo.CppHandlerIncludeName())

	// 如果有第一个特殊的 yourCode，先写入
	if firstCode != "" {
		data.WriteString(firstCode)
	}

	className := firstMethodInfo.Service() + config.HandlerFileName

	// 遍历 methodList，构建每个方法的处理函数
	for _, methodInfo := range *methodList {
		// 如果该方法有对应的 yourCode
		if code, exists := yourCodesMap[GenerateMethodHandlerKeyNameWrapper(methodInfo)]; exists {
			data.WriteString(fmt.Sprintf("void %s::%s(%sconst %s* request,\n",
				className, methodInfo.Method(), config.GoogleMethodController, methodInfo.CppRequest()))
			data.WriteString(config.Tab + "     " + methodInfo.CppResponse() + "* response,\n")
			data.WriteString(config.Tab + "     ::google::protobuf::Closure* done)\n")
			data.WriteString("{\n")
			data.WriteString(code) // 插入该方法的 yourCode
			data.WriteString("}\n\n")
		} else {
			data.WriteString(config.YourCodePair) // 如果没有 yourCode，则使用默认值
		}
	}

	return data.String()
}

func getMethodRepliedHandlerCppStr(dst string, methodList *RPCMethods) string {
	// Ensure there are methods in the list
	if len(*methodList) == 0 {
		return ""
	}

	// Read code sections from file (returns a map with method name as key and code as value)
	yourCodesMap, firstCode, _ := ReadCodeSectionsFromFile(dst, methodList, GenerateMethodHandlerKeyNameWrapper)

	var data strings.Builder
	firstMethodInfo := (*methodList)[0]

	// Start with the C++ replied handler include specific to the first method
	data.WriteString(firstMethodInfo.CppRepliedHandlerIncludeName())
	data.WriteString("#include \"network/codec/dispatcher.h\"\n\n")

	// External declaration for the dispatcher
	data.WriteString("extern ProtobufDispatcher gResponseDispatcher;\n\n")

	// 如果有第一个特殊的 yourCode，先写入
	if firstCode != "" {
		data.WriteString(firstCode)
	}

	var declarationData, implData strings.Builder

	// Iterate through methodList and construct the handler registration and implementation
	for _, methodInfo := range *methodList {
		// Check if there's code available for the current method
		if code, exists := yourCodesMap[methodInfo.KeyName()]; exists {
			// Construct function name for the handler
			funcName := "On" + methodInfo.KeyName() + config.RepliedHandlerFileName

			// Register message callback in declaration data
			declarationData.WriteString(fmt.Sprintf("%s%s", config.Tab,
				"gResponseDispatcher.registerMessageCallback<"+methodInfo.CppResponse()+
					">(std::bind(&"+funcName+", std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));\n"))

			// Implementation of the handler function
			implData.WriteString(fmt.Sprintf("void %s(const TcpConnectionPtr& conn, const std::shared_ptr<%s>& replied, Timestamp timestamp)\n{\n",
				funcName, methodInfo.CppResponse()))
			implData.WriteString(code) // Append the code for this method
			implData.WriteString("}\n\n")
		} else {
			// If there's no code for this method, you can handle this case however you like.
			// For now, we just append a comment or handle the missing code.
			data.WriteString(fmt.Sprintf("// No code found for method %s\n", methodInfo.Method()))
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

	// Read code sections from file (returns a map with method name as key and code as value)
	yourCodesMap, firstCode, _ := ReadCodeSectionsFromFile(dst, methodList, GenerateMethodHandlerNameWrapper)

	var data strings.Builder

	// Append the provided includeName
	data.WriteString(includeName)

	// 如果有第一个特殊的 yourCode，先写入
	if firstCode != "" {
		data.WriteString(firstCode)
	}

	// Iterate through methodList and construct handler functions for each method
	for _, methodInfo := range *methodList {
		// Check if there's code available for the current method
		if code, exists := yourCodesMap[GenerateMethodHandlerNameWrapper(methodInfo)]; exists {
			// Append method handler function definition
			data.WriteString(fmt.Sprintf("void %s::%s(%sconst %s* request,\n",
				className, methodInfo.Method(), config.PlayerMethodController, methodInfo.CppRequest()))
			data.WriteString(config.Tab + "     " + methodInfo.CppResponse() + "* response)\n")
			data.WriteString("{\n")
			data.WriteString(code) // Append the code for this method
			data.WriteString("}\n\n")
		} else {
			data.WriteString(config.YourCodePair)
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
		instanceData.WriteString(fmt.Sprintf("%sgNodeService.emplace(\"%s\", std::make_unique_for_overwrite<%s%s>());\n",
			config.Tab, firstMethodInfo.Service(), firstMethodInfo.Service(), config.HandlerFileName))
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

	isCommonOrLogicProto := strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.CommonProtoDirIndex]) ||
		strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.LogicProtoDirIndex])

	if strings.Contains(firstMethodInfo.Path(), config.PlayerName) ||
		strings.Contains(firstMethodInfo.FileNameNoEx(), config.PlayerName) {
		return false
	}

	hasGsPrefix := strings.HasPrefix(firstMethodInfo.FileNameNoEx(), config.GameNodePrefixName)

	return isCommonOrLogicProto && hasGsPrefix
}

func isGsPlayerHandler(methodList *RPCMethods) bool {
	if len(*methodList) <= 0 {
		return false
	}

	firstMethodInfo := (*methodList)[0]

	// Check if the method belongs to a player service
	if strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.ClientPlayerDirIndex]) {
		return true
	}

	// Check if the file base name contains player name and does not contain centre prefix
	fileBaseName := firstMethodInfo.FileNameNoEx()

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
	fileName := methodList[0].FileNameNoEx() + config.HandlerHeaderExtension

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
	fileName := strings.ToLower(methodList[0].FileNameNoEx()) + config.HandlerCppExtension
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
	fileName := methodList[0].FileNameNoEx() + config.HandlerHeaderExtension

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
	fileName := strings.ToLower(firstMethodInfo.FileNameNoEx()) + config.HandlerCppExtension
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

	if strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.ClientPlayerDirIndex]) {
		return false
	}

	// Check if the file base name does not contain the GameNodePrefixName
	return strings.Contains(firstMethodInfo.FileNameNoEx(), config.CentrePlayerPrefixName)
}

func writeGsPlayerMethodRepliedHandlerHeadFile(methodList RPCMethods) {
	defer util.Wg.Done()

	// Check if the method list qualifies as a game server player replied handler
	if !isGsPlayerRepliedHandler(&methodList) {
		return
	}

	// Generate the file name based on the first method's base name and configuration
	fileName := methodList[0].FileNameNoEx() + config.RepliedHandlerHeaderExtension

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
	fileName := strings.ToLower(firstMethodInfo.FileNameNoEx()) + config.CppRepliedHandlerEx
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
	if !(strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.CommonProtoDirIndex]) ||
		strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.LogicProtoDirIndex])) {
		return false
	}

	if strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.ClientPlayerDirIndex]) {
		return false
	}

	if !firstMethodInfo.CcGenericServices() {
		return false
	}

	// Check if the file base name contains specific keywords
	return strings.Contains(firstMethodInfo.FileNameNoEx(), config.CentrePrefixName) ||
		strings.Contains(firstMethodInfo.FileNameNoEx(), config.GatePrefixName)
}

func writeGsMethodRepliedHandlerHeadFile(methodList RPCMethods) {
	defer util.Wg.Done()

	if !isGsMethodRepliedHandler(&methodList) {
		return
	}

	fileBaseName := methodList[0].FileNameNoEx()
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

	fileBaseName := methodList[0].FileNameNoEx()
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
	isInCommonOrLogicProtoDir := strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.CommonProtoDirIndex]) ||
		strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.LogicProtoDirIndex])

	// 如果不在 common 或 logic proto 目录中，直接返回 false
	if !isInCommonOrLogicProtoDir {
		return false
	}

	if strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.ClientPlayerDirIndex]) {
		return false
	}

	// 如果路径或文件名包含 PlayerName，直接返回 false
	if strings.Contains(firstMethodInfo.Path(), config.PlayerName) ||
		strings.Contains(firstMethodInfo.FileNameNoEx(), config.PlayerName) {
		return false
	}

	// 检查文件名是否包含 Centre 前缀
	return strings.Contains(firstMethodInfo.FileNameNoEx(), config.CentrePrefixName)
}

func writeCentreMethodHandlerHeadFile(methodList RPCMethods) {
	defer util.Wg.Done()

	if !isCentreMethodHandler(&methodList) {
		return
	}

	firstMethodInfo := methodList[0]
	fileName := firstMethodInfo.FileNameNoEx() + config.HandlerHeaderExtension
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
	fileBaseName := firstMethodInfo.FileNameNoEx()
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
	return strings.Contains(firstMethodInfo.FileNameNoEx(), config.CentrePlayerPrefixName)
}

func writeCentrePlayerMethodHandlerHeadFile(methodList RPCMethods) {
	defer util.Wg.Done()

	if !isCentrePlayerHandler(&methodList) {
		return
	}

	firstMethodInfo := methodList[0]
	fileName := firstMethodInfo.FileNameNoEx() + config.HandlerHeaderExtension
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
	fileName := strings.ToLower(firstMethodInfo.FileNameNoEx()) + config.HandlerCppExtension
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
	return strings.Contains(firstMethodInfo.FileNameNoEx(), config.GameNodePrefixName) ||
		strings.Contains(firstMethodInfo.FileNameNoEx(), config.GatePrefixName)
}

func writeCentreMethodRepliedHandlerHeadFile(methodList RPCMethods) {
	defer util.Wg.Done()

	if !isCentreMethodRepliedHandler(&methodList) {
		return
	}

	firstMethodInfo := methodList[0]
	fileName := strings.ToLower(firstMethodInfo.FileNameNoEx()) + config.RepliedHandlerHeaderExtension
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
	fileName := strings.ToLower(firstMethodInfo.FileNameNoEx()) + config.CppRepliedHandlerEx
	dstFileName := config.CentreMethodRepliedHandleDir + fileName

	data := getMethodRepliedHandlerCppStr(dstFileName, &methodList)
	util.WriteMd5Data2File(dstFileName, data)
}

func isCentrePlayerRepliedHandler(methodList *RPCMethods) bool {
	if len(*methodList) == 0 {
		return false
	}

	firstMethodInfo := (*methodList)[0]

	if strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.ClientPlayerDirIndex]) {
		return false
	}

	// Check if it's a player service and not containing CentrePrefixName in ProtoFileBaseName
	return firstMethodInfo.IsPlayerService() && !strings.Contains(firstMethodInfo.FileNameNoEx(), config.CentrePrefixName)
}

func writeCentrePlayerMethodRepliedHandlerHeadFile(methodList RPCMethods) {
	defer util.Wg.Done() // defer execution of util.Wg.Done() to mark completion

	// Check if the CentrePlayer has replied handler for the methodList
	if !isCentrePlayerRepliedHandler(&methodList) {
		return // If not, return early
	}

	// Construct the file name using the base name of the first method in methodList
	fileName := methodList[0].FileNameNoEx() + config.RepliedHandlerHeaderExtension

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
	fileName := strings.ToLower(firstMethodInfo.FileNameNoEx()) + config.CppRepliedHandlerEx

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
	if !strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.CommonProtoDirIndex]) {
		return false
	}

	// Check if the method has CcGenericServices enabled
	if !firstMethodInfo.CcGenericServices() {
		return false
	}

	// Check if the ProtoFileBaseName of the method contains any of the specified prefixes
	return strings.Contains(firstMethodInfo.FileNameNoEx(), config.CentrePrefixName) ||
		strings.Contains(firstMethodInfo.FileNameNoEx(), config.DeployPrefixName) ||
		strings.Contains(firstMethodInfo.FileNameNoEx(), config.GameNodePrefixName) ||
		strings.Contains(firstMethodInfo.FileNameNoEx(), config.LoginPrefixName)
}

func isGateServiceHandler(methodList *RPCMethods) (check bool) {
	// Check if the methodList is empty
	if len(*methodList) <= 0 {
		return false
	}

	// Retrieve the first method information from methodList
	firstMethodInfo := (*methodList)[0]

	// Check if the method's path contains the common ProtoDirNames
	if !strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.CommonProtoDirIndex]) {
		return false
	}

	// Check if the ProtoFileBaseName of the method contains the substring "gate"
	return strings.Contains(firstMethodInfo.FileNameNoEx(), "gate")
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
	fileName := methodList[0].FileNameNoEx() + config.HandlerHeaderExtension

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
	fileName := strings.ToLower(methodList[0].FileNameNoEx()) + config.HandlerCppExtension
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
	fileName := strings.ToLower(methodList[0].FileNameNoEx()) + config.RepliedHandlerHeaderExtension
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
	fileName := strings.ToLower(firstMethodInfo.FileNameNoEx()) + config.CppRepliedHandlerEx
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
