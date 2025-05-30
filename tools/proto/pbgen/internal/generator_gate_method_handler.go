package internal

import (
	"log"
	"pbgen/config"
	"pbgen/util"
	"strings"
)

// 判断 methodList 是否为空
func isEmpty(methodList *RPCMethods) bool {
	return len(*methodList) == 0
}

// 判断 path 是否包含指定目录名
func containsDir(path string, dirIndex int) bool {
	return strings.Contains(path, config.ProtoDirectoryNames[dirIndex])
}

// 判断 methodList 第一个 method 是否满足给定条件函数
func checkFirstMethod(methodList *RPCMethods, conditions ...func(*MethodInfo) bool) bool {
	if isEmpty(methodList) {
		return false
	}
	first := (*methodList)[0]
	for _, cond := range conditions {
		if !cond(first) {
			return false
		}
	}
	return true
}

// 特定条件判断

func isGateServiceHandler(methodList *RPCMethods) bool {
	return checkFirstMethod(methodList, func(m *MethodInfo) bool {
		return containsDir(m.Path(), config.GateProtoDirIndex)
	})
}

func isGateMethodRepliedHandler(methodList *RPCMethods) bool {
	return checkFirstMethod(methodList,
		func(m *MethodInfo) bool {
			return containsDir(m.Path(), config.CommonProtoDirIndex)
		},
		func(m *MethodInfo) bool {
			return m.CcGenericServices()
		},
		func(m *MethodInfo) bool {
			return util.IsPathInOtherProtoDirs(m.Path(), config.GateProtoDirIndex)
		},
	)
}

// 通用写文件函数，传入判断条件和文件生成函数

func writeGateMethodFile(
	methodList RPCMethods,
	checkFunc func(*RPCMethods) bool,
	genFileName func(*MethodInfo) string,
	genData func(string, *RPCMethods) (string, error),
	outputDir string,
) {
	defer util.Wg.Done()

	if !checkFunc(&methodList) || len(methodList) == 0 {
		return
	}

	first := methodList[0]
	fileName := genFileName(first)
	dstFile := outputDir + fileName
	data, err := genData(dstFile, &methodList)
	if err != nil {
		log.Fatalf("writeGateMethodFile: genData error: %v", err)
	}

	util.WriteMd5Data2File(dstFile, data)
}

// 具体写调用

func writeGateMethodHandlerHeadFile(methodList RPCMethods) {
	writeGateMethodFile(
		methodList,
		isGateServiceHandler,
		func(m *MethodInfo) string { return m.FileNameNoEx() + config.HandlerHeaderExtension },
		func(_ string, list *RPCMethods) (string, error) { return getServiceHandlerHeadStr(*list) },
		config.GateMethodHandlerDirectory,
	)
}

func writeGateMethodHandlerCppFile(methodList RPCMethods) {
	writeGateMethodFile(
		methodList,
		isGateServiceHandler,
		func(m *MethodInfo) string { return strings.ToLower(m.FileNameNoEx()) + config.HandlerCppExtension },
		func(dst string, list *RPCMethods) (string, error) {
			return getMethodHandlerCppStr(dst, list), nil
		},
		config.GateMethodHandlerDirectory,
	)
}

func writeGateMethodRepliedHandlerHeadFile(methodList RPCMethods) {
	writeGateMethodFile(
		methodList,
		isGateMethodRepliedHandler,
		func(m *MethodInfo) string {
			return strings.ToLower(m.FileNameNoEx()) + config.RepliedHandlerHeaderExtension
		},
		func(_ string, list *RPCMethods) (string, error) { return getMethodRepliedHandlerHeadStr(*list) },
		config.GateMethodRepliedHandlerDirectory,
	)
}

func writeGateMethodRepliedHandlerCppFile(methodList RPCMethods) {
	writeGateMethodFile(
		methodList,
		isGateMethodRepliedHandler,
		func(m *MethodInfo) string { return strings.ToLower(m.FileNameNoEx()) + config.CppRepliedHandlerEx },
		func(dst string, list *RPCMethods) (string, error) {
			return getMethodRepliedHandlerCppStr(dst, *list, "", ""), nil
		},
		config.GateMethodRepliedHandlerDirectory,
	)
}
