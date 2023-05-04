package gen

import (
	"bufio"
	"gengo/config"
	"gengo/util"
	cases "golang.org/x/text/cases"
	"golang.org/x/text/language"
	"log"
	"os"
	"path/filepath"
	"strings"
)

func writeEventCppHandler(fd os.DirEntry, dstDir string) {
	util.Wg.Done()
	f, err := os.Open(config.ProtoDirs[config.EventProtoDirIndex] + fd.Name())
	if err != nil {
		return
	}
	defer f.Close()

	scanner := bufio.NewScanner(f)
	var line string
	var eventList []string
	for scanner.Scan() {
		line = scanner.Text()
		if !strings.Contains(line, "message") {
			continue
		}
		eventMessage := strings.Split(line, " ")[1]
		eventMessage = strings.Replace(eventMessage, "\n", "", -1)
		eventList = append(eventList, eventMessage)
	}

	dataHead := "#pragma once\n" + "#include \"src/game_logic/thread_local/thread_local_storage.h\"\n\n"

	className := strings.Split(fd.Name(), "_")[0]
	caString := cases.Title(language.English)
	className = caString.String(className) + "EventHandler"

	var classDeclareHeader string
	var registerFunctionBody string
	var unregisterFunctionBody string
	var handlerFunction string
	for _, s := range eventList {
		classDeclareHeader += "class " + s + ";\n"
		handlerFunction += config.Tab + "static void " + s + "Handler(const " + s + "& message);\n"
		registerFunctionBody += config.Tab2 + "dispatcher.sink<" + s + ">().connect<&" +
			className + "::" + s + "Handler>();\n"
		unregisterFunctionBody += config.Tab2 + "dispatcher.sink<" + s + ">().disconnect<&" +
			className + "::" + s + "Handler>();\n"
	}
	dataHead += classDeclareHeader + "\n"
	dataHead += "class " + className + "\n"
	dataHead += "{\npublic:\n"
	dataHead += config.Tab + "static void Register(entt::dispatcher& dispatcher)\n"
	dataHead += config.Tab + "{\n" + registerFunctionBody + config.Tab + "}\n\n"
	dataHead += config.Tab + "static void UnRegister(entt::dispatcher& dispatcher)\n"
	dataHead += config.Tab + "{\n" + unregisterFunctionBody + config.Tab + "}\n\n"
	dataHead += handlerFunction
	dataHead += "};\n"

	baseName := filepath.Base(strings.ToLower(fd.Name()))
	fileName := strings.Replace(dstDir+strings.ToLower(fd.Name()), config.ProtoEx, "", -1)
	headerFileName := fileName + config.HeadHandlerEx
	cppFileName := fileName + config.CppHandlerEx
	Md5WriteData2File(headerFileName, dataHead)

	dataCpp := config.IncludeBegin + filepath.Base(headerFileName) + config.IncludeEndLine +
		config.IncludeBegin + config.ProtoDirNames[config.EventProtoDirIndex] +
		strings.Replace(baseName, config.ProtoEx, config.ProtoPbhEx, -1) + config.IncludeEndLine
	dataCpp += config.YourCodePair

	var yourCodes []string
	fdCpp, errCpp := os.Open(cppFileName)
	if errCpp == nil {
		defer fdCpp.Close()
		scanner := bufio.NewScanner(f)
		var line string
		yourCodeIndex := 0
		for scanner.Scan() {
			line = scanner.Text()
			if strings.Contains(line, config.YourCodeBegin) {
				yourCodes = append(yourCodes, line)
			} else if strings.Contains(line, config.YourCodeEnd) {
				yourCodes = append(yourCodes, line)
				yourCodeIndex += 1
			} else if yourCodeIndex < len(yourCodes) {
				yourCodes[yourCodeIndex] += line
			}
		}
	} else {
		for i := 0; i < len(eventList)+1; i++ {
			yourCodes = append(yourCodes, config.YourCodePair)
		}
	}

	for i := 0; i < len(yourCodes); i++ {
		j := i - 1
		isEventIndex := j >= 0 && j < len(eventList)
		if isEventIndex {
			dataCpp += "void " + className + "::" + eventList[j] + "Handler(const " + eventList[j] + "& message)\n{\n"
		}
		dataCpp += yourCodes[i]
		if isEventIndex {
			dataCpp += "}\n\n"
		}
	}
	Md5WriteData2File(cppFileName, dataCpp)
}

func WriteEventHandlerFile() {
	fds, err := os.ReadDir(config.ProtoDirs[config.EventProtoDirIndex])
	if err != nil {
		log.Fatal(err)
		return
	}
	for _, fd := range fds {
		if !util.IsProtoFile(fd) {
			continue
		}
		util.Wg.Add(1)
		writeEventCppHandler(fd, config.GsEventHandleDir)
		util.Wg.Add(1)
		writeEventCppHandler(fd, config.ControllerEventHandleDir)
	}
}
