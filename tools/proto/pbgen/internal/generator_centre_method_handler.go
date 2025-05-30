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

func writeCentrePlayerMethodHandlerHeadFile(methods RPCMethods) {
	defer util.Wg.Done()

	if !isCentrePlayerHandler(&methods) {
		return
	}

	firstMethodInfo := methods[0]
	fileName := firstMethodInfo.FileNameNoEx() + config.HandlerHeaderExtension
	outputFilePath := config.CentreNodePlayerMethodHandlerDirectory + fileName

	data, err := getPlayerServiceHeadStr(methods)

	if err != nil {
		log.Fatal(err)
	}

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

	data := getPlayerServiceHandlerCppStr(
		dstFileName,
		methodList,
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

	if !(strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.GameProtoDirIndex]) ||
		strings.Contains(firstMethodInfo.Service(), config.ProtoDirectoryNames[config.GameProtoDirIndex])) {
		return false
	}

	if util.ContainsPlayerKeyword(firstMethodInfo.Service()) {
		return false
	}

	return true
}

func writeCentreMethodRepliedHandlerHeadFile(methodList RPCMethods) {
	defer util.Wg.Done()

	if !isCentreMethodRepliedHandler(&methodList) {
		return
	}

	firstMethodInfo := methodList[0]
	fileName := strings.ToLower(firstMethodInfo.FileNameNoEx()) + config.RepliedHandlerHeaderExtension
	dstFileName := config.CentreMethodRepliedHandleDir + fileName

	data, err := getServiceRepliedHandlerHeadStr(methodList)
	if err != nil {
		log.Fatal(err)
	}
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

	data := getServiceRepliedHandlerCppStr(dstFileName, methodList, "", "")

	util.WriteMd5Data2File(dstFileName, data)
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

func writeCentrePlayerMethodRepliedHandlerHeadFile(methodList RPCMethods) {
	defer util.Wg.Done() // defer execution of util.Wg.Done() to mark completion

	// Check if the CentrePlayer has replied handler for the methodList
	if !isCentrePlayerRepliedHandler(&methodList) {
		return // If not, return early
	}

	// Construct the file name using the base name of the first method in methodList
	fileName := methodList[0].FileNameNoEx() + config.RepliedHandlerHeaderExtension

	data, err := getPlayerMethodRepliedHeadStr(methodList)

	if err != nil {
		log.Fatal(err)
	}

	// Write the MD5 data to a file located in CentreMethodRepliedHandleDir
	util.WriteMd5Data2File(config.CentrePlayerMethodRepliedHandlerDirectory+fileName, data)
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
	data := getPlayerServiceHandlerCppStr(
		dstFileName,
		methodList,
		firstMethodInfo.CppRepliedHandlerClassName(),
		firstMethodInfo.CppRepliedHandlerIncludeName(),
	)

	// Write the generated C++ handler code to file
	util.WriteMd5Data2File(dstFileName, data)
}
