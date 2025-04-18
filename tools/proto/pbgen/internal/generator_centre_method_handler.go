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

func writeCentrePlayerMethodHandlerHeadFile(methods RPCMethods) {
	defer util.Wg.Done()

	if !isCentrePlayerHandler(&methods) {
		return
	}

	firstMethodInfo := methods[0]
	fileName := firstMethodInfo.FileNameNoEx() + config.HandlerHeaderExtension
	outputFilePath := config.CentreNodePlayerMethodHandlerDirectory + fileName

	data, err := getPlayerMethodHeadStr(methods)

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
