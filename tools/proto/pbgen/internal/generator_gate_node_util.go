package internal

import (
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
			return m.CcGenericServices()
		},
		func(info *MethodInfo) bool {
			return !(util.ContainsPlayerKeyword(info.Service()))
		},
		func(m *MethodInfo) bool {
			return !util.IsPathInProtoDirs(m.Path(), config.GateProtoDirIndex)
		},
	)
}
