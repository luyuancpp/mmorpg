package gen

import (
	"bufio"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"strings"

	"golang.org/x/text/cases"
	"golang.org/x/text/language"

	"pbgen/config"
	"pbgen/util"
)

// generateClassNameFromFile 从 proto 文件名生成 C++ 类名（下划线转为大写驼峰），加后缀
func generateClassNameFromFile(file os.DirEntry, suffix string) string {
	baseName := strings.TrimSuffix(file.Name(), filepath.Ext(file.Name()))
	parts := strings.Split(baseName, "_")
	title := cases.Title(language.English)

	var classParts []string
	for _, part := range parts {
		if part != "" {
			classParts = append(classParts, title.String(part))
		}
	}
	if len(classParts) == 0 {
		return "Default" + suffix
	}
	return strings.Join(classParts, "") + suffix
}

// parseProtoMessages 提取 proto 文件中的所有 message 名称
func parseProtoMessages(protoFilePath string) ([]string, error) {
	file, err := os.Open(protoFilePath)
	if err != nil {
		return nil, err
	}
	defer file.Close()

	var messages []string
	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		line := scanner.Text()
		if strings.HasPrefix(strings.TrimSpace(line), "message ") {
			parts := strings.Fields(line)
			if len(parts) > 1 {
				messages = append(messages, parts[1])
			}
		}
	}
	return messages, scanner.Err()
}

// buildEventHandlerSignature 构建事件处理函数的定义签名
func buildEventHandlerSignature(className, eventName string) string {
	return fmt.Sprintf("void %s::%sHandler(const %s& event)\n", className, eventName, eventName)
}

// extractUserCodeBlocks 提取 .cpp 文件中每个事件处理函数的自定义代码区域
func extractUserCodeBlocks(cppPath string, methodSignatures []string) (map[string]string, string, error) {
	codeMap := make(map[string]string)

	file, err := os.Open(cppPath)
	if err != nil {
		return nil, "", err
	}
	defer file.Close()

	scanner := bufio.NewScanner(file)
	var (
		globalCode    string
		currentMethod string
		currentCode   string
		inGlobalCode  bool
		inMethodCode  bool
		globalHandled bool
	)

	for scanner.Scan() {
		line := scanner.Text() + "\n"

		// 全局 yourCode 逻辑
		if inGlobalCode {
			globalCode += line
			if strings.Contains(line, config.YourCodeEnd) {
				inGlobalCode = false
			}
			continue
		}
		if !globalHandled && strings.Contains(line, config.YourCodeBegin) {
			globalCode = line
			inGlobalCode = true
			globalHandled = true
			continue
		}

		// 事件处理函数代码提取
		if currentMethod == "" {
			for _, signature := range methodSignatures {
				if strings.Contains(line, signature) {
					currentMethod = signature
					break
				}
			}
		}

		if currentMethod != "" {
			if strings.Contains(line, config.YourCodeBegin) {
				inMethodCode = true
				currentCode += line
			} else if strings.Contains(line, config.YourCodeEnd) {
				currentCode += line
				codeMap[currentMethod] = currentCode
				currentMethod = ""
				currentCode = ""
				inMethodCode = false
			} else if inMethodCode {
				currentCode += line
			}
		}
	}

	if globalCode == "" {
		globalCode = config.YourCodePair
	}

	// 补充未找到的函数
	for _, signature := range methodSignatures {
		if _, exists := codeMap[signature]; !exists {
			codeMap[signature] = config.YourCodePair
		}
	}
	return codeMap, globalCode, scanner.Err()
}

// generateEventHandlerFiles 为每个 proto 文件生成事件处理代码
func generateEventHandlerFiles(file os.DirEntry, outputDir string) {
	defer util.Wg.Done()

	protoFilePath := config.ProtoDirs[config.EventProtoDirIndex] + file.Name()
	messages, err := parseProtoMessages(protoFilePath)
	if err != nil {
		log.Printf("Failed to parse proto: %v\n", err)
		return
	}

	className := generateClassNameFromFile(file, config.ClassNameSuffix)
	baseName := strings.ToLower(strings.TrimSuffix(file.Name(), config.ProtoEx))
	filePathPrefix := outputDir + baseName
	headerFile := filePathPrefix + config.HandlerHeaderExtension
	cppFile := filePathPrefix + config.HandlerCppExtension

	// 构建函数签名列表
	var handlerSignatures []string
	for _, msg := range messages {
		handlerSignatures = append(handlerSignatures, buildEventHandlerSignature(className, msg))
	}

	// 提取已存在的用户代码
	userCodeBlocks, globalUserCode, _ := extractUserCodeBlocks(cppFile, handlerSignatures)

	// 生成 header 内容
	var headerBuilder strings.Builder
	headerBuilder.WriteString("#pragma once\n\n")
	for _, msg := range messages {
		headerBuilder.WriteString("class " + msg + ";\n")
	}
	headerBuilder.WriteString(fmt.Sprintf("\nclass %s\n{\npublic:\n", className))
	headerBuilder.WriteString(config.Tab + "static void Register();\n")
	headerBuilder.WriteString(config.Tab + "static void UnRegister();\n")
	for _, msg := range messages {
		headerBuilder.WriteString(config.Tab + fmt.Sprintf("static void %sHandler(const %s& event);\n", msg, msg))
	}
	headerBuilder.WriteString("};\n")
	util.WriteMd5Data2File(headerFile, headerBuilder.String())

	// 生成 cpp 内容
	var cppBuilder strings.Builder
	cppBuilder.WriteString(fmt.Sprintf("#include \"%s\"\n", filepath.Base(headerFile)))
	cppBuilder.WriteString(fmt.Sprintf("#include \"%s%s%s\"\n", config.ProtoDirName, config.ProtoDirectoryNames[config.EventProtoDirIndex], strings.Replace(file.Name(), config.ProtoEx, config.ProtoPbhEx, 1)))
	cppBuilder.WriteString("#include \"thread_local/storage.h\"\n")

	if globalUserCode != "" {
		cppBuilder.WriteString(globalUserCode)
	}

	// Register/Unregister 函数体
	cppBuilder.WriteString(fmt.Sprintf("void %s::Register()\n{\n", className))
	for _, msg := range messages {
		cppBuilder.WriteString(config.Tab + fmt.Sprintf("tls.dispatcher.sink<%s>().connect<&%s::%sHandler>();\n", msg, className, msg))
	}
	cppBuilder.WriteString("}\n\n")

	cppBuilder.WriteString(fmt.Sprintf("void %s::UnRegister()\n{\n", className))
	for _, msg := range messages {
		cppBuilder.WriteString(config.Tab + fmt.Sprintf("tls.dispatcher.sink<%s>().disconnect<&%s::%sHandler>();\n", msg, className, msg))
	}
	cppBuilder.WriteString("}\n\n")

	// 写入事件处理函数定义
	for _, msg := range messages {
		signature := buildEventHandlerSignature(className, msg)
		cppBuilder.WriteString(signature + "{\n")
		cppBuilder.WriteString(userCodeBlocks[signature])
		cppBuilder.WriteString("}\n\n")
	}

	util.WriteMd5Data2File(cppFile, cppBuilder.String())
}

// generateAllEventHandlers 生成所有事件处理器
func GenerateAllEventHandlers() {
	files, err := os.ReadDir(config.ProtoDirs[config.EventProtoDirIndex])
	if err != nil {
		log.Fatal(err)
	}

	for _, file := range files {
		if !util.IsProtoFile(file) {
			continue
		}
		util.Wg.Add(2)
		go generateEventHandlerFiles(file, config.GameNodeEventHandlerDirectory)
		go generateEventHandlerFiles(file, config.CentreNodeEventHandlerDirectory)
	}
}
