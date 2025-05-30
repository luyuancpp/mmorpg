package internal

import (
	"log"
	"pbgen/config"
	"pbgen/util"
	"strings"
)

// /gate
func isGateMethodRepliedHandler(methodList *RPCMethods) (check bool) {
	// Check if the methodList is empty
	if len(*methodList) <= 0 {
		return false
	}

	// Retrieve the first method information from methodList
	firstMethodInfo := (*methodList)[0]

	// Check if the method's path contains the common ProtoDirNames
	if !strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.CommonProtoDirIndex]) {
		return false
	}

	// Check if the method has CcGenericServices enabled
	if !firstMethodInfo.CcGenericServices() {
		return false
	}

	// Check if the ProtoFileBaseName of the method contains any of the specified prefixes
	return strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.CenterProtoDirIndex]) ||
		strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.GameProtoDirIndex]) ||
		strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.LoginProtoDirIndex])
}

func isGateServiceHandler(methodList *RPCMethods) (check bool) {
	if len(*methodList) <= 0 {
		return false
	}
	firstMethodInfo := (*methodList)[0]
	if !strings.Contains(firstMethodInfo.Path(), config.ProtoDirectoryNames[config.GateProtoDirIndex]) {
		return false
	}
	return true
}

func writeGateMethodHandlerHeadFile(methodList RPCMethods) {
	defer util.Wg.Done()

	// Check if methodList is empty
	if len(methodList) <= 0 {
		return
	}

	// Check if methodList qualifies as a gate service handler
	if !isGateServiceHandler(&methodList) {
		return
	}

	// Construct the file name for the header handler file
	fileName := methodList[0].FileNameNoEx() + config.HandlerHeaderExtension

	data, err := getServiceHandlerHeadStr(methodList)

	if err != nil {
		log.Fatal(err)
	}

	// Write the MD5 data to the header handler file
	util.WriteMd5Data2File(config.GateMethodHandlerDirectory+fileName, data)
}

func writeGateMethodHandlerCppFile(methodList RPCMethods) {
	defer util.Wg.Done()

	// Check if methodList is empty
	if len(methodList) <= 0 {
		return
	}

	// Check if methodList qualifies as a gate service handler
	if !isGateServiceHandler(&methodList) {
		return
	}

	// Construct the file name for the C++ handler file
	fileName := strings.ToLower(methodList[0].FileNameNoEx()) + config.HandlerCppExtension
	dstFileName := config.GateMethodHandlerDirectory + fileName

	// Generate the C++ handler code as a string
	data := getMethodHandlerCppStr(dstFileName, &methodList)

	// Write the generated C++ handler code to file
	util.WriteMd5Data2File(dstFileName, data)
}

func writeGateMethodRepliedHandlerHeadFile(methodList RPCMethods) {
	defer util.Wg.Done()

	// Check if methodList is empty
	if len(methodList) <= 0 {
		return
	}

	// Check if methodList qualifies as a gate method replied handler
	if !isGateMethodRepliedHandler(&methodList) {
		return
	}

	// Construct the file name for the replied handler header file
	fileName := strings.ToLower(methodList[0].FileNameNoEx()) + config.RepliedHandlerHeaderExtension
	dstFileName := config.GateMethodRepliedHandlerDirectory + fileName

	// Generate the header handler code as a string
	data := getMethodRepliedHandlerHeadStr(&methodList)

	// Write the generated header handler code to file
	util.WriteMd5Data2File(dstFileName, data)
}

func writeGateMethodRepliedHandlerCppFile(methodList RPCMethods) {
	defer util.Wg.Done()

	// Check if methodList qualifies as a gate method replied handler
	if !isGateMethodRepliedHandler(&methodList) {
		return
	}

	// Retrieve the first method information from methodList
	firstMethodInfo := methodList[0]

	// Construct the file name for the C++ replied handler file
	fileName := strings.ToLower(firstMethodInfo.FileNameNoEx()) + config.CppRepliedHandlerEx
	dstFileName := config.GateMethodRepliedHandlerDirectory + fileName

	// Generate the C++ replied handler code as a string
	data := getMethodRepliedHandlerCppStr(dstFileName, &methodList)

	// Write the generated C++ replied handler code to file
	util.WriteMd5Data2File(dstFileName, data)
}
