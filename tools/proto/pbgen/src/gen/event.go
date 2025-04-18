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

func generateClassNameFromFile(protoFile os.DirEntry, suffix string) string {
	baseName := strings.TrimSuffix(protoFile.Name(), filepath.Ext(protoFile.Name()))
	if baseName == "" {
		return "Default" + suffix
	}

	parts := strings.Split(baseName, "_")
	titleConverter := cases.Title(language.English)
	var classNameParts []string
	for _, part := range parts {
		if part != "" {
			classNameParts = append(classNameParts, titleConverter.String(part))
		}
	}

	if len(classNameParts) == 0 {
		return "Default" + suffix
	}

	return strings.Join(classNameParts, "") + suffix
}

// ReadCodeSectionsFromFile 函数接收一个函数作为参数，动态选择 A 或 B 方法
func extractUserCodeBlocks(filePath string, methodSignatures []string) (map[string]string, string, error) {
	codeMap := make(map[string]string)
	file, err := os.Open(filePath)
	if err != nil {
		return nil, "", fmt.Errorf("cannot open file %s: %v", filePath, err)
	}
	defer file.Close()

	scanner := bufio.NewScanner(file)
	var (
		firstCode       string
		currentCode     string
		currentMethod   string
		inFirstBlock    bool
		firstBlockFound bool
		inMethodBlock   bool
	)

	for scanner.Scan() {
		line := scanner.Text() + "\n"

		if inFirstBlock {
			firstCode += line
			if strings.Contains(line, config.YourCodeEnd) {
				inFirstBlock = false
			}
			continue
		}

		if !firstBlockFound && strings.Contains(line, config.YourCodeBegin) {
			firstCode = line
			inFirstBlock = true
			firstBlockFound = true
			continue
		}

		if currentMethod == "" {
			for _, method := range methodSignatures {
				if strings.Contains(line, method) {
					currentMethod = method
					break
				}
			}
		}

		if currentMethod != "" {
			if strings.Contains(line, config.YourCodeBegin) {
				inMethodBlock = true
				currentCode += line
			} else if strings.Contains(line, config.YourCodeEnd) {
				currentCode += line
				codeMap[currentMethod] = currentCode
				currentMethod = ""
				currentCode = ""
				inMethodBlock = false
			} else if inMethodBlock {
				currentCode += line
			}
		}
	}

	if firstCode == "" {
		firstCode = config.YourCodePair
	}

	for _, method := range methodSignatures {
		if _, exists := codeMap[method]; !exists {
			codeMap[method] = config.YourCodePair
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

	cppContent := config.IncludeBegin + filepath.Base(headerFilePath) + config.IncludeEndLine +
		config.IncludeBegin + config.ProtoDirName + config.ProtoDirectoryNames[config.EventProtoDirIndex] +
		strings.Replace(baseName, config.ProtoEx, config.ProtoPbhEx, -1) + config.IncludeEndLine +
		"#include \"thread_local/storage.h\"\n"

	userCodeBlocks, firstCode, err := extractUserCodeBlocks(cppFilePath, eventHandlerSignatures)
	if err != nil {
		log.Fatal(err)
	}

	if firstCode != "" {
		cppContent += firstCode
	}

	cppContent += "void " + className + "::Register()\n" +
		"{\n" + registerFunctionBody + "}\n\n"
	cppContent += "void " + className + "::UnRegister()\n" +
		"{\n" + unregisterFunctionBody + "}\n\n"

	for _, eventName := range eventMessages {
		// 如果该方法有对应的 yourCode
		eventHandlerFunctionName := buildEventHandlerSignature(className, eventName)
		if code, exists := userCodeBlocks[eventHandlerFunctionName]; exists {
			cppContent += eventHandlerFunctionName
			cppContent += "{\n"
			cppContent += code
			cppContent += "}\n\n"
		} else {
			cppContent += eventHandlerFunctionName
			cppContent += "{\n"
			cppContent += config.YourCodePair
			cppContent += "}\n\n"
		}
	}
	util.WriteMd5Data2File(cppFilePath, cppContent)
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
	for _, protoFile := range fds {
		if !util.IsProtoFile(protoFile) {
			continue
		}
		util.Wg.Add(2)
		go generateEventHandlerFiles(protoFile, config.GameNodeEventHandlerDirectory)
		go generateEventHandlerFiles(protoFile, config.CentreNodeEventHandlerDirectory)
		cppIncludeData += config.IncludeBegin +
			strings.Replace(filepath.Base(strings.ToLower(protoFile.Name())), config.ProtoEx, config.HandlerHeaderExtension, 1) +
			config.IncludeEndLine
		registerData += generateClassNameFromFile(protoFile, config.ClassNameSuffix) + "::Register();\n"
		unRegisterData += generateClassNameFromFile(protoFile, config.ClassNameSuffix) + "::UnRegister();\n"
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
