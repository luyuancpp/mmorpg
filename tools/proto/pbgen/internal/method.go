package internal

import (
	"bufio"
	"fmt"
	"os"
	"pbgen/config"
	"pbgen/util"
	"strconv"
	"strings"
)

import (
	"text/template"
)

// 用于填充模板的数据结构
type HandlerFuncData struct {
	ClassName      string
	Method         string
	ControllerType string
	RequestType    string
	ResponseType   string
	UserCode       string
}

// 加载模板并应用
func generateHandlerFunc(outputPath string, method *RPCMethod, userCode string) error {
	// 加载模板文件
	tmpl, err := template.ParseFiles("templates/handler_func.tmpl")
	if err != nil {
		return err
	}

	// 准备模板数据
	data := HandlerFuncData{
		ClassName:      method.Service() + config.HandlerFileName,
		Method:         method.Method(),
		ControllerType: config.GoogleMethodController,
		RequestType:    method.CppRequest(),
		ResponseType:   method.CppResponse(),
		UserCode:       userCode,
	}

	// 创建输出文件
	file, err := os.Create(outputPath)
	if err != nil {
		return err
	}
	defer file.Close()

	// 执行模板
	return tmpl.ExecuteTemplate(file, "handler_func", data)
}

// Type definitions for callback functions
type checkRepliedCb func(methods *RPCMethods) bool

// Function to write the header file for service ID
func writeServiceIdHeadFile(methods RPCMethods) {
	defer util.Wg.Done()

	if len(methods) <= 0 {
		return
	}

	firstMethod := methods[0]

	if !firstMethod.CcGenericServices() {
		return
	}

	fileName := methods[0].FileNameNoEx() + config.ServiceInfoExtension + config.HeaderExtension
	util.WriteMd5Data2File(config.ServiceInfoDirectory+fileName, GenServiceIdHeader(methods))
}

func GenServiceIdHeader(methods RPCMethods) string {
	if len(methods) == 0 {
		return ""
	}
	var b strings.Builder
	b.WriteString("#pragma once\n#include <cstdint>\n\n")
	b.WriteString(methods[0].IncludeName() + "\n")

	for _, method := range methods {
		b.WriteString(fmt.Sprintf("constexpr uint32_t %s%s = %d;\n", method.KeyName(), config.MessageIdName, method.Id))
		b.WriteString(fmt.Sprintf("constexpr uint32_t %sIndex = %d;\n", method.KeyName(), method.Index))
		b.WriteString(fmt.Sprintf("#define %sMethod  ::%s_Stub::descriptor()->method(%d)\n\n", method.KeyName(), method.Service, method.Index))
	}
	return b.String()
}

// Function to get the header string for service handlers
func getServiceHandlerHeadStr(methods RPCMethods) string {
	var data strings.Builder
	data.WriteString("#pragma once\n")
	data.WriteString(methods[0].IncludeName())
	data.WriteString("class " + methods[0].Service() + "Handler : public ::" + methods[0].Service() + "\n{\npublic:\n")

	for _, method := range methods {
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
func getPlayerMethodHeadStr(methods RPCMethods) string {
	var data strings.Builder
	data.WriteString("#pragma once\n")
	data.WriteString(methods[0].IncludeName())
	data.WriteString(config.PlayerServiceIncludeName)
	data.WriteString(config.MacroReturnIncludeName)
	data.WriteString("\nclass " + methods[0].Service() + config.HandlerFileName + " : public ::PlayerService" + "\n{\npublic:\n")
	data.WriteString(config.Tab + "using PlayerService::PlayerService;\n")

	data.WriteString(getPlayerMethodHandlerFunctions(methods))
	data.WriteString("\n};\n")

	return data.String()
}

// Helper function to generate method handler functions for player methods
func getPlayerMethodHandlerFunctions(methods RPCMethods) string {
	var data strings.Builder
	var callFunctionList strings.Builder

	for i, method := range methods {
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
func getPlayerMethodRepliedHeadStr(methods RPCMethods) string {
	var data strings.Builder
	data.WriteString("#pragma once\n")
	data.WriteString(methods[0].IncludeName())
	data.WriteString(config.PlayerServiceRepliedIncludeName)
	data.WriteString("\nclass " + methods[0].Service() + config.RepliedHandlerFileName + " : public ::PlayerServiceReplied" + "\n{\npublic:\n")
	data.WriteString(config.Tab + "using PlayerServiceReplied::PlayerServiceReplied;\n")

	data.WriteString(getPlayerMethodRepliedHandlerFunctions(methods))
	data.WriteString("\n};\n")

	return data.String()
}

// Helper function to generate method replied handler functions for player methods
func getPlayerMethodRepliedHandlerFunctions(methods RPCMethods) string {
	var data strings.Builder
	var callFunctionList strings.Builder

	for i, method := range methods {
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

func getMethodRepliedHandlerHeadStr(methods *RPCMethods) string {
	// Ensure there are methods in the list
	if len(*methods) == 0 {
		return ""
	}

	var data strings.Builder
	firstMethodInfo := (*methods)[0]

	// Start with pragma once and include the first method's specific include if available
	data.WriteString("#pragma once\n")
	data.WriteString(firstMethodInfo.IncludeName())
	data.WriteString("#include \"muduo/net/TcpConnection.h\"\n\n")
	data.WriteString("using namespace muduo;\n")
	data.WriteString("using namespace muduo::net;\n\n")

	// Generate handler function declarations for each method
	for _, methodInfo := range *methods {
		handlerDeclaration := fmt.Sprintf("void On%s%s(const TcpConnectionPtr& conn, const std::shared_ptr<%s>& replied, Timestamp timestamp);\n\n",
			methodInfo.KeyName(), config.RepliedHandlerFileName, methodInfo.CppResponse())
		data.WriteString(handlerDeclaration)
	}

	return data.String()
}

// ReadCodeSectionsFromFile 函数接收一个函数作为参数，动态选择 A 或 B 方法
func ReadCodeSectionsFromFile(cppFileName string, methods *RPCMethods, methodFunc func(info *RPCMethod, funcParam string) string, funcParam string) (map[string]string, string, error) {
	// 创建一个 map 来存储每个 RPCMethod 的 name 和对应的 yourCode
	codeMap := make(map[string]string)

	// 打开文件
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
			for _, method := range *methods {
				handlerName := methodFunc(method, funcParam)
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
				handlerName := methodFunc(currentMethod, funcParam)
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
	for _, method := range *methods {
		handlerName := methodFunc(method, funcParam)
		if _, exists := codeMap[handlerName]; !exists {
			codeMap[handlerName] = config.YourCodePair
		}
	}

	return codeMap, firstCode, nil
}

func GenerateMethodHandlerNameWrapper(info *RPCMethod, _ string) string {
	return info.Service() + config.HandlerFileName + "::" + info.Method() + "("
}

func GenerateMethodHandlerNameWithClassPrefixWrapper(info *RPCMethod, classPrefix string) string {
	return classPrefix + "::" + info.Method() + "("
}

func GenerateMethodHandlerKeyNameWrapper(info *RPCMethod, _ string) string {
	return "On" + info.KeyName() + config.RepliedHandlerFileName
}

func getMethodHandlerCppStr(dst string, methods *RPCMethods) string {
	// 确保方法列表非空
	if len(*methods) == 0 {
		return ""
	}

	ex := ""

	// 读取代码块
	yourCodesMap, firstCode, _ := ReadCodeSectionsFromFile(dst, methods, GenerateMethodHandlerNameWrapper, ex)

	var data strings.Builder
	firstMethodInfo := (*methods)[0]

	// 写入 C++ 处理函数的头部
	data.WriteString(firstMethodInfo.CppHandlerIncludeName())

	// 如果有第一个特殊的 yourCode，先写入
	if firstCode != "" {
		data.WriteString(firstCode)
	}

	// 遍历 methods，构建每个方法的处理函数
	for _, methodInfo := range *methods {
		// 如果该方法有对应的 yourCode
		if code, exists := yourCodesMap[GenerateMethodHandlerNameWrapper(methodInfo, ex)]; exists {
			data.WriteString(fmt.Sprintf("void %s%sconst %s* request,\n",
				GenerateMethodHandlerNameWrapper(methodInfo, ex), config.GoogleMethodController, methodInfo.CppRequest()))
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

func getMethodRepliedHandlerCppStr(dst string, methods *RPCMethods) string {
	// Ensure there are methods in the list
	if len(*methods) == 0 {
		return ""
	}

	emtpyString := ""

	// Read code sections from file (returns a map with method name as key and code as value)
	yourCodesMap, firstCode, _ := ReadCodeSectionsFromFile(dst, methods, GenerateMethodHandlerKeyNameWrapper, emtpyString)

	var data strings.Builder
	firstMethodInfo := (*methods)[0]

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

	// Iterate through methods and construct the handler registration and implementation
	for _, methodInfo := range *methods {
		// Check if there's code available for the current method
		if code, exists := yourCodesMap[GenerateMethodHandlerKeyNameWrapper(methodInfo, emtpyString)]; exists {
			// Construct function name for the handler
			funcName := GenerateMethodHandlerKeyNameWrapper(methodInfo, emtpyString)

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

func getMethodPlayerHandlerCppStr(dst string, methods *RPCMethods, className string, includeName string) string {
	// Ensure there are methods in the list
	if len(*methods) == 0 {
		return includeName // Return the includeName if no methods are provided
	}

	// Read code sections from file (returns a map with method name as key and code as value)
	yourCodesMap, firstCode, _ := ReadCodeSectionsFromFile(dst, methods, GenerateMethodHandlerNameWithClassPrefixWrapper, className)

	var data strings.Builder

	// Append the provided includeName
	data.WriteString(includeName)

	// 如果有第一个特殊的 yourCode，先写入
	if firstCode != "" {
		data.WriteString(firstCode)
	}

	// Iterate through methods and construct handler functions for each method
	for _, methodInfo := range *methods {
		// Check if there's code available for the current method
		if code, exists := yourCodesMap[GenerateMethodHandlerNameWithClassPrefixWrapper(methodInfo, className)]; exists {
			// Append method handler function definition
			data.WriteString(fmt.Sprintf("void %s%sconst %s* request,\n",
				GenerateMethodHandlerNameWithClassPrefixWrapper(methodInfo, className), config.PlayerMethodController, methodInfo.CppRequest()))
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
		methods, ok := ServiceMethodMap[key]
		if !ok {
			continue
		}
		if !cb(&methods) {
			continue
		}
		firstMethodInfo := methods[0]

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
		methods, ok := ServiceMethodMap[key]
		if !ok {
			continue
		}
		if !cb(&methods) {
			continue
		}
		firstMethodInfo := methods[0]

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
func isGsMethodHandler(methods *RPCMethods) bool {
	if len(*methods) == 0 {
		return false
	}

	firstMethodInfo := (*methods)[0]

	isCommonOrLogicProto := strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.CommonProtoDirIndex]) ||
		strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.LogicProtoDirIndex])

	if strings.Contains(firstMethodInfo.Path(), config.PlayerName) ||
		strings.Contains(firstMethodInfo.FileNameNoEx(), config.PlayerName) {
		return false
	}

	hasGsPrefix := strings.HasPrefix(firstMethodInfo.FileNameNoEx(), config.GameNodePrefixName)

	return isCommonOrLogicProto && hasGsPrefix
}

func isGsPlayerHandler(methods *RPCMethods) bool {
	if len(*methods) <= 0 {
		return false
	}

	firstMethodInfo := (*methods)[0]

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
