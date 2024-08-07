package gen

import (
	"bufio"
	"fmt"
	"golang.org/x/text/cases"
	"golang.org/x/text/language"
	"log"
	"os"
	"path/filepath"
	"pbgen/config"
	"pbgen/util"
	"strings"
)

// getClassName generates the event handler class name based on the file name.
func getClassName(fd os.DirEntry) string {
	className := strings.Split(fd.Name(), "_")[0]
	caString := cases.Title(language.English)
	className = caString.String(className) + "EventHandler"
	return className
}

// writeEventHandlerCpp generates the C++ header and implementation files for event handling.
func writeEventHandlerCpp(fd os.DirEntry, dstDir string) {
	util.Wg.Done()

	// Read the proto file to extract event messages.
	eventList, err := extractEventList(fd)
	if err != nil {
		log.Printf("Error extracting event list from %s: %v\n", fd.Name(), err)
		return
	}

	// Generate the header content.
	className := getClassName(fd)
	headerContent := generateHeaderContent(fd, className, eventList)

	// Write header file.
	headerFileName := generateFileName(fd, dstDir, config.HeadHandlerEx)
	util.WriteMd5Data2File(headerFileName, headerContent)

	// Generate and write C++ implementation content.
	cppContent := generateCppContent(fd, className, eventList, headerFileName)
	cppFileName := generateFileName(fd, dstDir, config.CppHandlerEx)
	util.WriteMd5Data2File(cppFileName, cppContent)
}

// extractEventList reads the proto file and extracts the event messages.
func extractEventList(fd os.DirEntry) ([]string, error) {
	var eventList []string
	f, err := os.Open(config.ProtoDirs[config.EventProtoDirIndex] + fd.Name())
	if err != nil {
		return eventList, fmt.Errorf("failed to open file %s: %v", fd.Name(), err)
	}
	defer f.Close()

	scanner := bufio.NewScanner(f)
	for scanner.Scan() {
		line := scanner.Text()
		if strings.Contains(line, "message") {
			eventMessage := strings.Split(line, " ")[1]
			eventMessage = strings.Replace(eventMessage, "\n", "", -1)
			eventList = append(eventList, eventMessage)
		}
	}
	if err := scanner.Err(); err != nil {
		return eventList, fmt.Errorf("error scanning file %s: %v", fd.Name(), err)
	}

	return eventList, nil
}

// generateHeaderContent generates the header file content.
func generateHeaderContent(fd os.DirEntry, className string, eventList []string) string {
	dataHead := "#pragma once\n\n"

	var classDeclareHeader strings.Builder
	var handlerFunction strings.Builder
	for _, s := range eventList {
		classDeclareHeader.WriteString("class " + s + ";\n")
		handlerFunction.WriteString(config.Tab + "static void " + s + "Handler(const " + s + "& event);\n")
	}

	dataHead += classDeclareHeader.String() + "\n"
	dataHead += "class " + className + "\n"
	dataHead += "{\npublic:\n"
	dataHead += config.Tab + "static void Register();\n"
	dataHead += config.Tab + "static void UnRegister();\n\n"
	dataHead += handlerFunction.String()
	dataHead += "};\n"

	return dataHead
}

// generateCppContent generates the C++ implementation content.
func generateCppContent(fd os.DirEntry, className string, eventList []string, headerFileName string) string {
	dataCpp := config.IncludeBegin + filepath.Base(headerFileName) + config.IncludeEndLine +
		config.IncludeBegin + config.ProtoDirNames[config.EventProtoDirIndex] +
		strings.Replace(fd.Name(), config.ProtoEx, config.ProtoPbhEx, -1) + config.IncludeEndLine +
		"#include \"thread_local/storage.h\"\n"

	yourCodes, _ := util.ReadCodeSectionsFromFile(generateFileName(fd, "", config.CppHandlerEx), len(eventList)+1)

	for i := 0; i < len(yourCodes); i++ {
		j := i - 1
		isEventIndex := j >= 0 && j < len(eventList)
		if j == 0 {
			dataCpp += "void " + className + "::Register()\n" +
				"{\n" + generateRegisterUnregisterBody(eventList, className, true) + "}\n\n"
			dataCpp += "void " + className + "::UnRegister()\n" +
				"{\n" + generateRegisterUnregisterBody(eventList, className, false) + "}\n\n"
		}
		if isEventIndex {
			dataCpp += "void " + className + "::" + eventList[j] + "Handler(const " + eventList[j] + "& event)\n{\n"
		}
		dataCpp += yourCodes[i]
		if isEventIndex {
			dataCpp += "}\n\n"
		}
	}
	return dataCpp
}

// generateRegisterUnregisterBody generates the body for Register and Unregister functions.
func generateRegisterUnregisterBody(eventList []string, className string, isRegister bool) string {
	var functionBody strings.Builder
	action := "connect" // Default to connect for Register

	if !isRegister {
		action = "disconnect"
	}

	for _, s := range eventList {
		functionBody.WriteString(config.Tab2 + "tls.dispatcher.sink<" + s + ">()." + action + "<&" +
			className + "::" + s + "Handler>();\n")
	}

	return functionBody.String()
}

// generateFileName generates the full file name based on the directory, file name, and extension.
func generateFileName(fd os.DirEntry, dstDir string, extension string) string {
	baseName := filepath.Base(strings.ToLower(fd.Name()))
	fileName := strings.Replace(dstDir+strings.ToLower(fd.Name()), config.ProtoEx, "", -1)
	return fileName + extension
}

// WriteEventHandlerFile iterates over event proto files and writes corresponding C++ header and implementation files.
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
	util.WriteMd5Data2File(config.GsEventHandleDir+config.EventHandlerFileNameHead, eventHeadData)
	util.WriteMd5Data2File(config.CentreEventHandleDir+config.EventHandlerFileNameHead, eventHeadData)

	eventCppData := config.IncludeBegin + config.EventHandlerFileNameHead + config.IncludeEndLine
	eventCppData += cppIncludeData
	eventCppData += "void EventHandler::Register()\n{\n"
	eventCppData += registerData
	eventCppData += "}\n"
	eventCppData += "void EventHandler::UnRegister()\n{\n"
	eventCppData += unRegisterData
	eventCppData += "}\n"
	util.WriteMd5Data2File(config.GsEventHandleDir+config.EventHandlerFileNameCpp, eventCppData)
	util.WriteMd5Data2File(config.CentreEventHandleDir+config.EventHandlerFileNameCpp, eventCppData)
}
