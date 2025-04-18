package gen

import (
	"bufio"
	"fmt"
	cases "golang.org/x/text/cases"
	"golang.org/x/text/language"
	"log"
	"os"
	"path/filepath"
	"pbgen/config"
	"pbgen/util"
	"strings"
)

// generateClassNameFromFile 根据文件名生成类名，支持所有部分首字母大写，并追加后缀
func generateClassNameFromFile(fd os.DirEntry, suffix string) string {
	// 获取文件名
	name := fd.Name()
	if name == "" {
		return ""
	}

	// 移除文件扩展名
	baseName := strings.TrimSuffix(name, filepath.Ext(name))

	// 按 "_" 分隔文件名
	parts := strings.Split(baseName, "_")

	// 初始化 Title 转换器
	titleConverter := cases.Title(language.English)

	// 遍历每部分，将非空部分的首字母大写
	var capitalizedParts []string
	for _, part := range parts {
		if part != "" { // 跳过空字符串
			capitalizedParts = append(capitalizedParts, titleConverter.String(part))
		}
	}

	// 如果没有有效部分，返回默认类名
	if len(capitalizedParts) == 0 {
		return "Default" + suffix
	}

	// 拼接所有部分并加上后缀
	return strings.Join(capitalizedParts, "") + suffix
}

// ReadCodeSectionsFromFile 函数接收一个函数作为参数，动态选择 A 或 B 方法
func extractUserCodeBlocks(cppFileName string, methodList []string) (map[string]string, string, error) {
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
	var currentMethod string
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

		if "" == currentMethod {
			// 如果是方法的开始行，检查是否是我们关心的 RPCMethod 名称
			for _, method := range methodList {
				handlerName := method
				if strings.Contains(line, handlerName) {
					currentMethod = method
					break
				}
			}
		}

		// 如果找到了当前方法的开始，接着读取直到找到结束
		if currentMethod != "" {
			if strings.Contains(line, config.YourCodeBegin) {
				inMethodCode = true
				currentCode += line
			} else if strings.Contains(line, config.YourCodeEnd) {
				currentCode += line
				// 使用 methodFunc currentMethod
				handlerName := currentMethod
				codeMap[handlerName] = currentCode
				currentMethod = ""
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
	for _, method := range methodList {
		handlerName := method
		if _, exists := codeMap[handlerName]; !exists {
			codeMap[handlerName] = config.YourCodePair
		}
	}

	return codeMap, firstCode, nil
}

func buildEventHandlerSignature(className string, eventName string) string {
	return "void " + className + "::" + eventName + "Handler(const " + eventName + "& event)\n"
}

func generateEventHandlerFiles(protoFile os.DirEntry, dstDir string) {
	util.Wg.Done()

	var eventMessages []string
	{
		f, err := os.Open(config.ProtoDirs[config.EventProtoDirIndex] + protoFile.Name())
		if err != nil {
			return
		}
		defer f.Close()
		scanner := bufio.NewScanner(f)
		var line string
		for scanner.Scan() {
			line = scanner.Text()
			if !strings.Contains(line, "message") {
				continue
			}
			eventMessage := strings.Split(line, " ")[1]
			eventMessage = strings.Replace(eventMessage, "\n", "", -1)
			eventMessages = append(eventMessages, eventMessage)
		}
	}

	dataHead := "#pragma once\n\n"

	className := generateClassNameFromFile(protoFile, config.ClassNameSuffix)

	var eventHandlerSignatures []string

	var classDeclareHeader string
	var registerFunctionBody string
	var unregisterFunctionBody string
	var handlerFunction string
	for _, eventName := range eventMessages {
		classDeclareHeader += "class " + eventName + ";\n"
		handlerFunction += config.Tab + "static void " + eventName + "Handler(const " + eventName + "& event);\n"
		registerFunctionBody += config.Tab + "tls.dispatcher.sink<" + eventName + ">().connect<&" +
			className + "::" + eventName + "Handler>();\n"
		unregisterFunctionBody += config.Tab + "tls.dispatcher.sink<" + eventName + ">().disconnect<&" +
			className + "::" + eventName + "Handler>();\n"

		eventHandlerSignatures = append(eventHandlerSignatures, buildEventHandlerSignature(className, eventName))
	}

	dataHead += classDeclareHeader + "\n"
	dataHead += "class " + className + "\n"
	dataHead += "{\npublic:\n"
	dataHead += config.Tab + "static void Register();\n"
	dataHead += config.Tab + "static void UnRegister();\n\n"
	dataHead += handlerFunction
	dataHead += "};\n"

	baseName := filepath.Base(strings.ToLower(protoFile.Name()))
	fileName := strings.Replace(dstDir+strings.ToLower(protoFile.Name()), config.ProtoEx, "", -1)
	headerFilePath := fileName + config.HandlerHeaderExtension
	cppFilePath := fileName + config.HandlerCppExtension

	util.WriteMd5Data2File(headerFilePath, dataHead)

	dataCpp := config.IncludeBegin + filepath.Base(headerFilePath) + config.IncludeEndLine +
		config.IncludeBegin + config.ProtoDirName + config.ProtoDirectoryNames[config.EventProtoDirIndex] +
		strings.Replace(baseName, config.ProtoEx, config.ProtoPbhEx, -1) + config.IncludeEndLine +
		"#include \"thread_local/storage.h\"\n"

	userCodeBlocks, firstCode, err := extractUserCodeBlocks(cppFilePath, eventHandlerSignatures)
	if err != nil {
		log.Fatal(err)
	}

	if firstCode != "" {
		dataCpp += firstCode
	}

	dataCpp += "void " + className + "::Register()\n" +
		"{\n" + registerFunctionBody + "}\n\n"
	dataCpp += "void " + className + "::UnRegister()\n" +
		"{\n" + unregisterFunctionBody + "}\n\n"

	for _, eventName := range eventMessages {
		// 如果该方法有对应的 yourCode
		eventHandlerFunctionName := buildEventHandlerSignature(className, eventName)
		if code, exists := userCodeBlocks[eventHandlerFunctionName]; exists {
			dataCpp += eventHandlerFunctionName
			dataCpp += "{\n"
			dataCpp += code
			dataCpp += "}\n\n"
		} else {
			dataCpp += eventHandlerFunctionName
			dataCpp += "{\n"
			dataCpp += config.YourCodePair
			dataCpp += "}\n\n"
		}
	}
	util.WriteMd5Data2File(cppFilePath, dataCpp)
}

func GenerateAllEventHandlers() {
	fds, err := os.ReadDir(config.ProtoDirs[config.EventProtoDirIndex])
	if err != nil {
		log.Fatal(err)
		return
	}

	var cppIncludeData string
	var registerData string
	var unRegisterData string
	for _, fd := range fds {
		if !util.IsProtoFile(fd) {
			continue
		}
		util.Wg.Add(1)
		generateEventHandlerFiles(fd, config.GameNodeEventHandlerDirectory)
		util.Wg.Add(1)
		generateEventHandlerFiles(fd, config.CentreNodeEventHandlerDirectory)
		cppIncludeData += config.IncludeBegin +
			strings.Replace(filepath.Base(strings.ToLower(fd.Name())), config.ProtoEx, config.HandlerHeaderExtension, 1) +
			config.IncludeEndLine
		registerData += generateClassNameFromFile(fd, config.ClassNameSuffix) + "::Register();\n"
		unRegisterData += generateClassNameFromFile(fd, config.ClassNameSuffix) + "::UnRegister();\n"
	}
	eventHeadData := "#pragma once\n\n"
	eventHeadData += "class EventHandler\n{\npublic:\n"
	eventHeadData += "static void Register();\n"
	eventHeadData += "static void UnRegister();\n"
	eventHeadData += "};\n"
	util.WriteMd5Data2File(config.GameNodeEventHandlerDirectory+config.EventHandlerHeaderFileName, eventHeadData)
	util.WriteMd5Data2File(config.CentreNodeEventHandlerDirectory+config.EventHandlerHeaderFileName, eventHeadData)

	eventCppData := config.IncludeBegin + config.EventHandlerHeaderFileName + config.IncludeEndLine
	eventCppData += cppIncludeData
	eventCppData += "void EventHandler::Register()\n{\n"
	eventCppData += registerData
	eventCppData += "}\n"
	eventCppData += "void EventHandler::UnRegister()\n{\n"
	eventCppData += unRegisterData
	eventCppData += "}\n"
	util.WriteMd5Data2File(config.GameNodeEventHandlerDirectory+config.EventHandlerCppFileName, eventCppData)
	util.WriteMd5Data2File(config.CentreNodeEventHandlerDirectory+config.EventHandlerCppFileName, eventCppData)
}
