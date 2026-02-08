package cpp

import (
	"bufio"
	"fmt"
	"golang.org/x/text/cases"
	"golang.org/x/text/language"
	"os"
	"path/filepath"
	"strings"
	"sync"

	"go.uber.org/zap" // 引入zap，用于日志字段

	_config "pbgen/internal/config"
	utils2 "pbgen/internal/utils"
	"pbgen/logger" // 引入全局logger包
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
			if strings.Contains(line, _config.Global.Naming.YourCodeEnd) {
				inGlobalCode = false
			}
			continue
		}
		if !globalHandled && strings.Contains(line, _config.Global.Naming.YourCodeBegin) {
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
			if strings.Contains(line, _config.Global.Naming.YourCodeBegin) {
				inMethodCode = true
				currentCode += line
			} else if strings.Contains(line, _config.Global.Naming.YourCodeEnd) {
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
		globalCode = _config.Global.Naming.YourCodePair
	}

	// 补充未找到的函数
	for _, signature := range methodSignatures {
		if _, exists := codeMap[signature]; !exists {
			codeMap[signature] = _config.Global.Naming.YourCodePair
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
func generateEventHandlerFiles(wg *sync.WaitGroup, file os.DirEntry, outputDir string) {
	defer wg.Done()

	protoFilePath := _config.Global.PathLists.ProtoDirs.LogicEvent + file.Name()
	eventMessages, err := parseProtoMessages(protoFilePath)
	if err != nil {
		logger.Global.Error("解析proto文件失败",
			zap.String("proto_file", protoFilePath),
			zap.Error(err),
		)
		return
	}

	className := generateClassNameFromFile(file, _config.Global.Naming.HandlerFile)
	baseName := strings.ToLower(strings.TrimSuffix(file.Name(), _config.Global.FileExtensions.Proto))
	filePrefix := outputDir + baseName

	headerFilePath := filePrefix + _config.Global.FileExtensions.HandlerH
	cppFilePath := filePrefix + _config.Global.FileExtensions.HandlerCpp
	headerFileBase := filepath.Base(headerFilePath)

	// 构建 handler 签名
	var handlerSignatures []string
	for _, evt := range eventMessages {
		handlerSignatures = append(handlerSignatures, buildEventHandlerSignature(className, evt))
	}

	// 提取自定义用户代码块
	userCodeBlocks, globalCode, err := extractUserCodeBlocks(cppFilePath, handlerSignatures)
	if err != nil {
		logger.Global.Warn("读取用户代码失败",
			zap.String("cpp_file", cppFilePath),
			zap.Error(err),
		)
	}

	// 构建模板数据
	tmplData := EventTemplateData{
		ClassName:           className,
		HeaderFile:          headerFileBase,
		ProtoInclude:        _config.Global.DirectoryNames.ProtoDirName + _config.Global.PathLists.ProtoDirs.LogicEvent + strings.Replace(file.Name(), _config.Global.FileExtensions.Proto, _config.Global.FileExtensions.PbH, 1),
		EventMessages:       eventMessages,
		ForwardDeclarations: eventMessages,
		GlobalUserCode:      globalCode,
		UserCodeBlocks:      userCodeBlocks,
	}

	// 渲染模板并写入文件
	if err := utils2.RenderTemplateToFile("internal/template/event_handler.h.tmpl", headerFilePath, tmplData); err != nil {
		logger.Global.Error("生成头文件失败",
			zap.String("header_file", headerFilePath),
			zap.Error(err),
		)
	}
	if err := utils2.RenderTemplateToFile("internal/template/event_handler.cpp.tmpl", cppFilePath, tmplData); err != nil {
		logger.Global.Error("生成cpp文件失败",
			zap.String("cpp_file", cppFilePath),
			zap.Error(err),
		)
	}
}

// generateAllEventHandlers 生成所有事件处理器
func GenerateAllEventHandlers(wg *sync.WaitGroup) {
	files, err := os.ReadDir(_config.Global.PathLists.ProtoDirs.LogicEvent)
	if err != nil {
		logger.Global.Fatal("读取proto目录失败",
			zap.String("dir", _config.Global.PathLists.ProtoDirs.LogicEvent),
			zap.Error(err),
		)
	}

	for _, file := range files {
		if !utils2.IsProtoFile(file) {
			continue
		}
		wg.Add(2)
		go generateEventHandlerFiles(wg, file, _config.Global.Paths.RoomNodeEventHandlerDirectory)
		go generateEventHandlerFiles(wg, file, _config.Global.Paths.CentreNodeEventHandlerDirectory)
	}

	wg.Add(1)
	go func() {
		defer wg.Done()
		err = GenerateAllEventHandlersTemplate(files)
		if err != nil {
			logger.Global.Fatal("生成事件处理器模板失败",
				zap.Error(err),
			)
			return
		}
	}()
}

type Config struct {
	ProtoDirs                       []string
	EventProtoDirIndex              int
	ProtoEx                         string
	HandlerHeaderExtension          string
	EventHandlerHeaderFileName      string
	EventHandlerCppFileName         string
	RoomNodeEventHandlerDirectory   string
	CentreNodeEventHandlerDirectory string
	IncludeBegin                    string
	IncludeEndLine                  string
	ClassNameSuffix                 string
}

func GenerateAllEventHandlersTemplate(protoFiles []os.DirEntry) error {
	// Template for the header file content
	const eventHandlerHeaderTemplate = `#pragma once

class EventHandler
{
public:
	static void Register();
	static void UnRegister();
};
`

	// Prepare the dynamic data
	var cppIncludeData, registerData, unRegisterData string
	for _, protoFile := range protoFiles {
		// Only process valid proto files
		if !strings.HasSuffix(protoFile.Name(), _config.Global.FileExtensions.Proto) {
			continue
		}

		// Include header file name with the right extension
		cppIncludeData += _config.Global.Naming.IncludeBegin + strings.Replace(filepath.Base(strings.ToLower(protoFile.Name())), _config.Global.FileExtensions.Proto, _config.Global.FileExtensions.HandlerH, 1) + _config.Global.Naming.IncludeEndLine

		// Register and UnRegister data
		className := generateClassNameFromFile(protoFile, _config.Global.Naming.HandlerFile)
		registerData += className + "::Register();\n"
		unRegisterData += className + "::UnRegister();\n"
	}

	// Create header file content
	eventHeadData := eventHandlerHeaderTemplate

	// Prepare the data for C++ source file
	eventCppData := struct {
		IncludeBegin               string
		EventHandlerHeaderFileName string
		IncludeEndLine             string
		CppIncludeData             string
		RegisterData               string
		UnRegisterData             string
	}{
		IncludeBegin:               _config.Global.Naming.IncludeBegin,
		EventHandlerHeaderFileName: _config.Global.Naming.EventHandlerBase + _config.Global.FileExtensions.Header,
		IncludeEndLine:             _config.Global.Naming.IncludeEndLine,
		CppIncludeData:             cppIncludeData,
		RegisterData:               registerData,
		UnRegisterData:             unRegisterData,
	}

	headerFilePath := _config.Global.Paths.RoomNodeEventHandlerDirectory + _config.Global.Naming.EventHandlerBase + _config.Global.FileExtensions.Header
	cppFilePath := _config.Global.Paths.RoomNodeEventHandlerDirectory + _config.Global.Naming.EventHandlerBase + _config.Global.FileExtensions.Cpp
	if err := utils2.RenderTemplateToFile("internal/template/event_handler_total.h.tmpl", headerFilePath, eventHeadData); err != nil {
		logger.Global.Error("生成总头文件失败",
			zap.String("header_file", headerFilePath),
			zap.Error(err),
		)
	}
	if err := utils2.RenderTemplateToFile("internal/template/event_handler_total.cpp.tmpl", cppFilePath, eventCppData); err != nil {
		logger.Global.Error("生成总cpp文件失败",
			zap.String("cpp_file", cppFilePath),
			zap.Error(err),
		)
	}

	headerFilePath = _config.Global.Paths.CentreNodeEventHandlerDirectory + _config.Global.Naming.EventHandlerBase + _config.Global.FileExtensions.Header
	cppFilePath = _config.Global.Paths.CentreNodeEventHandlerDirectory + _config.Global.Naming.EventHandlerBase + _config.Global.FileExtensions.Cpp
	if err := utils2.RenderTemplateToFile("internal/template/event_handler_total.h.tmpl", headerFilePath, eventHeadData); err != nil {
		logger.Global.Error("生成中心节点总头文件失败",
			zap.String("header_file", headerFilePath),
			zap.Error(err),
		)
	}
	if err := utils2.RenderTemplateToFile("internal/template/event_handler_total.cpp.tmpl", cppFilePath, eventCppData); err != nil {
		logger.Global.Error("生成中心节点总cpp文件失败",
			zap.String("cpp_file", cppFilePath),
			zap.Error(err),
		)
	}
	return nil
}
