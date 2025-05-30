package internal

import (
	"log"
	"pbgen/config"
	"pbgen/util"
	"strings"
)

func isCentreMethodHandler(methodList *RPCMethods) bool {
	if len(*methodList) == 0 {
		return false
	}

	firstMethodInfo := (*methodList)[0]

	if !strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.CenterProtoDirIndex]) {
		return false
	}

	if util.ContainsPlayerKeyword(firstMethodInfo.Service()) {
		return false
	}

	return true
}

func writeCentreMethodHandlerHeadFile(methodList RPCMethods) {
	defer util.Wg.Done()

	if !isCentreMethodHandler(&methodList) {
		return
	}

	firstMethodInfo := methodList[0]
	fileName := firstMethodInfo.FileNameNoEx() + config.HandlerHeaderExtension
	dstFileName := config.CentreNodeMethodHandlerDirectory + fileName

	data, err := getServiceHandlerHeadStr(methodList)

	if err != nil {
		log.Fatal(err)
	}

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

	data := getServiceHandlerCppStr(dstFileName, methodList, "", "")
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

func isCentreMethodRepliedHandler(methodList *RPCMethods) bool {
	if len(*methodList) == 0 {
		return false
	}

	firstMethodInfo := (*methodList)[0]

	if !(strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.GameProtoDirIndex]) ||
		strings.Contains(firstMethodInfo.Service(), config.ProtoDirectoryNames[config.GameProtoDirIndex])) {
		return false
	}

	if util.ContainsPlayerKeyword(firstMethodInfo.Service()) {
		return false
	}

	return true
}

func isCentrePlayerRepliedHandler(methodList *RPCMethods) bool {
	if len(*methodList) == 0 {
		return false
	}

	firstMethodInfo := (*methodList)[0]

	if !(strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.GameProtoDirIndex])) {
		return false
	}

	if !(util.ContainsPlayerKeyword(firstMethodInfo.Service())) {
		return false
	}

	if strings.Contains(firstMethodInfo.Service(), config.ClientPrefixName) {
		return false
	}

	return true
}
