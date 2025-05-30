package internal

import (
	"log"
	"pbgen/config"
	"pbgen/util"
	"strings"
)

// / game server
func IsGsMethodHandler(methods *RPCMethods) bool {
	if len(*methods) == 0 {
		return false
	}

	firstMethodInfo := (*methods)[0]

	if !strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.GameProtoDirIndex]) {
		return false
	}

	if strings.Contains(firstMethodInfo.Path(), config.TypePlayer) ||
		strings.Contains(firstMethodInfo.FileNameNoEx(), config.TypePlayer) {
		return false
	}

	return true
}

func IsGsPlayerHandler(methods *RPCMethods) bool {
	if len(*methods) <= 0 {
		return false
	}

	firstMethodInfo := (*methods)[0]

	// Check if the method belongs to a player service
	if !strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.GameProtoDirIndex]) {
		return true
	}

	if !strings.Contains(firstMethodInfo.Service(), config.DisplayPlayer) {
		return false
	}

	return true
}

func ReturnNoHandler(methods *RPCMethods) bool {
	return false
}

func writeGsMethodHandlerHeadFile(methods RPCMethods) {
	defer util.Wg.Done()

	// Check if the method list qualifies as a game server method handler
	if !IsGsMethodHandler(&methods) {
		return
	}

	// Generate the file name based on the first method's base name and configuration
	fileName := methods[0].FileNameNoEx() + config.HandlerHeaderExtension

	data, err := getServiceHandlerHeadStr(methods)

	if err != nil {
		log.Fatal(err)
	}

	// Write the generated data to the destination file using util.WriteMd5Data2File
	util.WriteMd5Data2File(config.GameNodeMethodHandlerDirectory+fileName, data)
}

func writeGsMethodHandlerCppFile(methodList RPCMethods) {
	defer util.Wg.Done()

	// Check if the method list qualifies as a game server method handler
	if !IsGsMethodHandler(&methodList) {
		return
	}

	// Generate the file name based on the first method's base name and configuration
	fileName := strings.ToLower(methodList[0].FileNameNoEx()) + config.HandlerCppExtension
	dstFileName := config.GameNodeMethodHandlerDirectory + fileName

	// Generate the C++ handler file content
	data := getMethodHandlerCppStr(dstFileName, &methodList)

	// Write the generated data to the destination file using util.WriteMd5Data2File
	util.WriteMd5Data2File(dstFileName, data)
}

func writeGsPlayerMethodHandlerHeadFile(methods RPCMethods) {
	defer util.Wg.Done()

	// Check if the method list qualifies as a game server player method handler
	if !IsGsPlayerHandler(&methods) {
		return
	}

	// Generate the file name based on the first method's base name and configuration
	fileName := methods[0].FileNameNoEx() + config.HandlerHeaderExtension

	data, err := getPlayerMethodHeadStr(methods)

	if err != nil {
		log.Fatal(err)
	}
	// Write the generated data to the destination file using util.WriteMd5Data2File
	util.WriteMd5Data2File(config.GameNodePlayerMethodHandlerDirectory+fileName, data)
}

func writeGsPlayerMethodHandlerCppFile(methodList RPCMethods) {
	defer util.Wg.Done()

	// Check if methodList is empty or does not qualify as a game server player method handler
	if len(methodList) <= 0 || !IsGsPlayerHandler(&methodList) {
		return
	}

	firstMethodInfo := methodList[0]

	// Generate the file name based on the first method's base name and configuration
	fileName := strings.ToLower(firstMethodInfo.FileNameNoEx()) + config.HandlerCppExtension
	dstFileName := config.GameNodePlayerMethodHandlerDirectory + fileName

	// Generate the C++ handler file content
	data := getMethodPlayerHandlerCppStr(dstFileName,
		&methodList,
		firstMethodInfo.CppHandlerClassName(),
		firstMethodInfo.CppHandlerIncludeName())

	// Write the generated data to the destination file using util.WriteMd5Data2File
	util.WriteMd5Data2File(dstFileName, data)
}

func isGsPlayerRepliedHandler(methodList *RPCMethods) bool {
	if len(*methodList) <= 0 {
		return false
	}

	firstMethodInfo := (*methodList)[0]

	if strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.GameProtoDirIndex]) {
		return false
	}

	// Check if the file base name does not contain the GameNodePrefixName
	return strings.Contains(firstMethodInfo.FileNameNoEx(), config.CentrePlayerPrefixName)
}

func writeGsPlayerMethodRepliedHandlerHeadFile(methodList RPCMethods) {
	defer util.Wg.Done()

	// Check if the method list qualifies as a game server player replied handler
	if !isGsPlayerRepliedHandler(&methodList) {
		return
	}

	// Generate the file name based on the first method's base name and configuration
	fileName := methodList[0].FileNameNoEx() + config.RepliedHandlerHeaderExtension

	data, err := getPlayerMethodRepliedHeadStr(methodList)

	if err != nil {
		log.Fatal(err)
	}

	// Write the generated data to the destination file using util.WriteMd5Data2File
	util.WriteMd5Data2File(config.GameNodePlayerMethodRepliedHandlerDirectory+fileName, data)
}

func writeGsPlayerMethodRepliedHandlerCppFile(methodList RPCMethods) {
	defer util.Wg.Done()

	// Check if methodList is empty or does not qualify as a game server player replied handler
	if len(methodList) <= 0 || !isGsPlayerRepliedHandler(&methodList) {
		return
	}

	firstMethodInfo := methodList[0]

	// Generate the file name based on the first method's base name and configuration
	fileName := strings.ToLower(firstMethodInfo.FileNameNoEx()) + config.CppRepliedHandlerEx
	dstFileName := config.GameNodePlayerMethodRepliedHandlerDirectory + fileName

	// Generate the C++ replied handler file content
	data := getMethodPlayerHandlerCppStr(dstFileName,
		&methodList,
		firstMethodInfo.CppRepliedHandlerClassName(),
		firstMethodInfo.CppRepliedHandlerIncludeName())

	// Write the generated data to the destination file using util.WriteMd5Data2File
	util.WriteMd5Data2File(dstFileName, data)
}

func isGsMethodRepliedHandler(methodList *RPCMethods) bool {
	if len(*methodList) == 0 {
		return false
	}

	firstMethodInfo := (*methodList)[0]

	// Check if the method is from a valid path and has generic services enabled
	if !(strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.CenterProtoDirIndex]) ||
		strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.GateProtoDirIndex])) {
		return false
	}

	if !firstMethodInfo.CcGenericServices() {
		return false
	}

	// Check if the file base name contains specific keywords
	return strings.Contains(firstMethodInfo.FileNameNoEx(), config.CentrePrefixName) ||
		strings.Contains(firstMethodInfo.FileNameNoEx(), config.GatePrefixName)
}

func writeGsMethodRepliedHandlerHeadFile(methodList RPCMethods) {
	defer util.Wg.Done()

	if !isGsMethodRepliedHandler(&methodList) {
		return
	}

	fileBaseName := methodList[0].FileNameNoEx()
	fileName := strings.ToLower(fileBaseName) + config.RepliedHandlerHeaderExtension
	dstFileName := config.GameNodeMethodRepliedHandlerDirectory + fileName
	data := getMethodRepliedHandlerHeadStr(&methodList)
	util.WriteMd5Data2File(dstFileName, data)
}

func writeGsMethodRepliedHandlerCppFile(methodList RPCMethods) {
	defer util.Wg.Done()

	if !isGsMethodRepliedHandler(&methodList) {
		return
	}

	fileBaseName := methodList[0].FileNameNoEx()
	fileName := strings.ToLower(fileBaseName) + config.CppRepliedHandlerEx
	dstFileName := config.GameNodeMethodRepliedHandlerDirectory + fileName
	data := getMethodRepliedHandlerCppStr(dstFileName, &methodList)
	util.WriteMd5Data2File(dstFileName, data)
}
