package gen

import (
	"bufio"
	cases "golang.org/x/text/cases"
	"golang.org/x/text/language"
	"log"
	"os"
	"path/filepath"
	"pbgen/config"
	"pbgen/util"
	"strings"
)

// getClassName 根据文件名生成类名，支持所有部分首字母大写，并追加后缀
func getClassName(fd os.DirEntry, suffix string) string {
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

func writeEventHandlerCpp(fd os.DirEntry, dstDir string) {
	util.Wg.Done()

	var eventList []string
	{
		f, err := os.Open(config.ProtoDirs[config.EventProtoDirIndex] + fd.Name())
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
			eventList = append(eventList, eventMessage)
		}
	}

	dataHead := "#pragma once\n\n"

	className := getClassName(fd, config.ClassNameSuffix)

	var classDeclareHeader string
	var registerFunctionBody string
	var unregisterFunctionBody string
	var handlerFunction string
	for _, s := range eventList {
		classDeclareHeader += "class " + s + ";\n"
		handlerFunction += config.Tab + "static void " + s + "Handler(const " + s + "& event);\n"
		registerFunctionBody += config.Tab2 + "tls.dispatcher.sink<" + s + ">().connect<&" +
			className + "::" + s + "Handler>();\n"
		unregisterFunctionBody += config.Tab2 + "tls.dispatcher.sink<" + s + ">().disconnect<&" +
			className + "::" + s + "Handler>();\n"
	}
	dataHead += classDeclareHeader + "\n"
	dataHead += "class " + className + "\n"
	dataHead += "{\npublic:\n"
	dataHead += config.Tab + "static void Register();\n"
	dataHead += config.Tab + "static void UnRegister();\n\n"
	dataHead += handlerFunction
	dataHead += "};\n"

	baseName := filepath.Base(strings.ToLower(fd.Name()))
	fileName := strings.Replace(dstDir+strings.ToLower(fd.Name()), config.ProtoEx, "", -1)
	headerFileName := fileName + config.HandlerHeaderExtension
	cppFileName := fileName + config.HandlerCppExtension
	util.WriteMd5Data2File(headerFileName, dataHead)

	dataCpp := config.IncludeBegin + filepath.Base(headerFileName) + config.IncludeEndLine +
		config.IncludeBegin + config.ProtoDirectoryNames[config.EventProtoDirIndex] +
		strings.Replace(baseName, config.ProtoEx, config.ProtoPbhEx, -1) + config.IncludeEndLine +
		"#include \"thread_local/storage.h\"\n"

	yourCodes, _ := util.ReadCodeSectionsFromFile(cppFileName, len(eventList)+1)

	for i := 0; i < len(yourCodes); i++ {
		j := i - 1
		isEventIndex := j >= 0 && j < len(eventList)
		if j == 0 {
			dataCpp += "void " + className + "::Register()\n" +
				"{\n" + registerFunctionBody + "}\n\n"
			dataCpp += "void " + className + "::UnRegister()\n" +
				"{\n" + unregisterFunctionBody + "}\n\n"
		}
		if isEventIndex {
			dataCpp += "void " + className + "::" + eventList[j] + "Handler(const " + eventList[j] + "& event)\n{\n"
		}
		dataCpp += yourCodes[i]
		if isEventIndex {
			dataCpp += "}\n\n"
		}
	}
	util.WriteMd5Data2File(cppFileName, dataCpp)
}

func WriteEventHandlerFile() {
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
		writeEventHandlerCpp(fd, config.GameNodeEventHandlerDirectory)
		util.Wg.Add(1)
		writeEventHandlerCpp(fd, config.CentreNodeEventHandlerDirectory)
		cppIncludeData += config.IncludeBegin +
			strings.Replace(filepath.Base(strings.ToLower(fd.Name())), config.ProtoEx, config.HandlerHeaderExtension, 1) +
			config.IncludeEndLine
		registerData += getClassName(fd, config.ClassNameSuffix) + "::Register();\n"
		unRegisterData += getClassName(fd, config.ClassNameSuffix) + "::UnRegister();\n"
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
