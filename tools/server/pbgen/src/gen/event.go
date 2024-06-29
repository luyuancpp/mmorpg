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

func getClassName(fd os.DirEntry) string {
	className := strings.Split(fd.Name(), "_")[0]
	caString := cases.Title(language.English)
	className = caString.String(className) + "EventHandler"
	return className
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

	className := getClassName(fd)

	var classDeclareHeader string
	var registerFunctionBody string
	var unregisterFunctionBody string
	var handlerFunction string
	for _, s := range eventList {
		classDeclareHeader += "class " + s + ";\n"
		handlerFunction += config.Tab + "static void " + s + "Handler(const " + s + "& message);\n"
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
	headerFileName := fileName + config.HeadHandlerEx
	cppFileName := fileName + config.CppHandlerEx
	WriteMd5Data2File(headerFileName, dataHead)

	dataCpp := config.IncludeBegin + filepath.Base(headerFileName) + config.IncludeEndLine +
		config.IncludeBegin + config.ProtoDirNames[config.EventProtoDirIndex] +
		strings.Replace(baseName, config.ProtoEx, config.ProtoPbhEx, -1) + config.IncludeEndLine +
		"#include \"thread_local/thread_local_storage.h\"\n"

	yourCodes, _ := util.GetDstCodeData(cppFileName, len(eventList)+1)

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
			dataCpp += "void " + className + "::" + eventList[j] + "Handler(const " + eventList[j] + "& message)\n{\n"
		}
		dataCpp += yourCodes[i]
		if isEventIndex {
			dataCpp += "}\n\n"
		}
	}
	WriteMd5Data2File(cppFileName, dataCpp)
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
		writeEventHandlerCpp(fd, config.GsEventHandleDir)
		util.Wg.Add(1)
		writeEventHandlerCpp(fd, config.CentreEventHandleDir)
		cppIncludeData += config.IncludeBegin +
			strings.Replace(filepath.Base(strings.ToLower(fd.Name())), config.ProtoEx, config.HeadHandlerEx, 1) +
			config.IncludeEndLine
		registerData += getClassName(fd) + "::Register();\n"
		unRegisterData += getClassName(fd) + "::UnRegister();\n"
	}
	eventHeadData := "#pragma once\n\n"
	eventHeadData += "class EventHandler\n{\npublic:\n"
	eventHeadData += "static void Register();\n"
	eventHeadData += "static void UnRegister();\n"
	eventHeadData += "};\n"
	WriteMd5Data2File(config.GsEventHandleDir+config.EventHandlerFileNameHead, eventHeadData)
	WriteMd5Data2File(config.CentreEventHandleDir+config.EventHandlerFileNameHead, eventHeadData)

	eventCppData := config.IncludeBegin + config.EventHandlerFileNameHead + config.IncludeEndLine
	eventCppData += cppIncludeData
	eventCppData += "void EventHandler::Register()\n{\n"
	eventCppData += registerData
	eventCppData += "}\n"
	eventCppData += "void EventHandler::UnRegister()\n{\n"
	eventCppData += unRegisterData
	eventCppData += "}\n"
	WriteMd5Data2File(config.GsEventHandleDir+config.EventHandlerFileNameCpp, eventCppData)
	WriteMd5Data2File(config.CentreEventHandleDir+config.EventHandlerFileNameCpp, eventCppData)
}
