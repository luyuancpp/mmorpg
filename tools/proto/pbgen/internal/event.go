package internal

import (
	"bufio"
	"fmt"
	"golang.org/x/text/cases"
	"golang.org/x/text/language"
	"log"
	"os"
	"path/filepath"
	"strings"

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

// EventTemplateData 用于渲染模板的数据结构
type EventTemplateData struct {
	ClassName           string
	HeaderFile          string
	ProtoInclude        string
	EventMessages       []string
	ForwardDeclarations []string
	GlobalUserCode      string
	UserCodeBlocks      map[string]string
}

// generateEventHandlerFiles 使用模板生成每个 proto 对应的 .h 和 .cpp 文件
func generateEventHandlerFiles(file os.DirEntry, outputDir string) {
	defer util.Wg.Done()

	protoFilePath := config.ProtoDirs[config.EventProtoDirIndex] + file.Name()
	eventMessages, err := parseProtoMessages(protoFilePath)
	if err != nil {
		log.Printf("failed to parse proto: %v\n", err)
		return
	}

	className := generateClassNameFromFile(file, config.ClassNameSuffix)
	baseName := strings.ToLower(strings.TrimSuffix(file.Name(), config.ProtoEx))
	filePrefix := outputDir + baseName

	headerFilePath := filePrefix + config.HandlerHeaderExtension
	cppFilePath := filePrefix + config.HandlerCppExtension
	headerFileBase := filepath.Base(headerFilePath)

	// 构建 handler 签名
	var handlerSignatures []string
	for _, evt := range eventMessages {
		handlerSignatures = append(handlerSignatures, buildEventHandlerSignature(className, evt))
	}

	// 提取自定义用户代码块
	userCodeBlocks, globalCode, err := extractUserCodeBlocks(cppFilePath, handlerSignatures)
	if err != nil {
		log.Printf("warning: failed to read user code from %s: %v\n", cppFilePath, err)
	}

	// 构建模板数据
	tmplData := EventTemplateData{
		ClassName:           className,
		HeaderFile:          headerFileBase,
		ProtoInclude:        config.ProtoDirName + config.ProtoDirectoryNames[config.EventProtoDirIndex] + strings.Replace(file.Name(), config.ProtoEx, config.ProtoPbhEx, 1),
		EventMessages:       eventMessages,
		ForwardDeclarations: eventMessages,
		GlobalUserCode:      globalCode,
		UserCodeBlocks:      userCodeBlocks,
	}

	// 渲染模板并写入文件
	if err := renderTemplateToFile("internal/gen/template/event_handler.h.tmpl", headerFilePath, tmplData); err != nil {
		log.Printf("failed to generate header file: %v\n", err)
	}
	if err := renderTemplateToFile("internal/gen/template/event_handler.cpp.tmpl", cppFilePath, tmplData); err != nil {
		log.Printf("failed to generate cpp file: %v\n", err)
	}
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
