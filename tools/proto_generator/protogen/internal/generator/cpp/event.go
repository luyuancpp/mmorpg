package cpp

import (
	"bufio"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"sync"

	"golang.org/x/text/cases"
	"golang.org/x/text/language"

	"go.uber.org/zap"

	_config "protogen/internal/config"
	utils2 "protogen/internal/utils"
	"protogen/logger"
)

// generateClassNameFromFile generates a C++ class name from a proto file name (underscore to upper camel case) with a suffix.
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

// parseProtoMessages extracts all message names from a proto file.
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

func parseProtoPackage(protoFilePath string) (string, error) {
	file, err := os.Open(protoFilePath)
	if err != nil {
		return "", err
	}
	defer file.Close()

	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		if !strings.HasPrefix(line, "package ") {
			continue
		}

		packageName := strings.TrimPrefix(line, "package ")
		packageName = strings.TrimSuffix(packageName, ";")
		return strings.TrimSpace(packageName), nil
	}

	return "", scanner.Err()
}

func qualifyProtoType(packageName, typeName string) string {
	if packageName == "" {
		return typeName
	}

	parts := strings.Split(packageName, ".")
	return strings.Join(parts, "::") + "::" + typeName
}

// buildEventHandlerSignature builds the definition signature for an event handler function.
func buildEventHandlerSignature(className, qualifiedTypeName, eventName string) string {
	return fmt.Sprintf("void %s::%sHandler(const %s& event)\n", className, eventName, qualifiedTypeName)
}

// extractUserCodeBlocks extracts user-written code blocks from each event handler function in a .cpp file.
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
	globalCode = utils2.TrimTrailingLineBreaks(globalCode)

	for _, signature := range methodSignatures {
		if _, exists := codeMap[signature]; !exists {
			codeMap[signature] = utils2.TrimTrailingLineBreaks(_config.Global.Naming.YourCodePair)
			continue
		}
		codeMap[signature] = utils2.TrimTrailingLineBreaks(codeMap[signature])
	}
	return codeMap, globalCode, scanner.Err()
}

// EventTemplateData holds data for rendering templates.
type EventTemplateData struct {
	ClassName      string
	HeaderFile     string
	ProtoInclude   string
	EventMessages  []EventHandlerMessage
	GlobalUserCode string
	UserCodeBlocks map[string]string
}

type EventHandlerMessage struct {
	Name          string
	QualifiedName string
	Signature     string
	IsLast        bool
}

// generateEventHandlerFiles generates .h and .cpp files for each proto using templates.
func generateEventHandlerFiles(wg *sync.WaitGroup, file os.DirEntry, protoRelativeDir string, outputDir string) {
	defer wg.Done()

	protoFilePath := filepath.Join(_config.Global.Paths.ProtoDir, protoRelativeDir, file.Name())
	eventMessages, err := parseProtoMessages(protoFilePath)
	if err != nil {
		logger.Global.Error("Failed to parse proto file",
			zap.String("proto_file", protoFilePath),
			zap.Error(err),
		)
		return
	}

	packageName, err := parseProtoPackage(protoFilePath)
	if err != nil {
		logger.Global.Warn("Failed to parse proto package, treating as no namespace",
			zap.String("proto_file", protoFilePath),
			zap.Error(err),
		)
		packageName = ""
	}

	className := generateClassNameFromFile(file, _config.Global.Naming.HandlerFile)
	baseName := strings.ToLower(strings.TrimSuffix(file.Name(), _config.Global.FileExtensions.Proto))
	filePrefix := filepath.Join(outputDir, baseName)

	headerFilePath := filePrefix + _config.Global.FileExtensions.HandlerH
	cppFilePath := filePrefix + _config.Global.FileExtensions.HandlerCpp
	headerFileBase := filepath.Base(headerFilePath)

	var handlerSignatures []string
	for _, evt := range eventMessages {
		handlerSignatures = append(handlerSignatures, buildEventHandlerSignature(className, qualifyProtoType(packageName, evt), evt))
	}

	userCodeBlocks, globalCode, err := extractUserCodeBlocks(cppFilePath, handlerSignatures)
	if err != nil {
		logger.Global.Warn("Failed to read user code",
			zap.String("cpp_file", cppFilePath),
			zap.Error(err),
		)
	}

	eventHandlers := make([]EventHandlerMessage, 0, len(eventMessages))
	for index, eventName := range eventMessages {
		qualifiedName := qualifyProtoType(packageName, eventName)
		eventHandlers = append(eventHandlers, EventHandlerMessage{
			Name:          eventName,
			QualifiedName: qualifiedName,
			Signature:     buildEventHandlerSignature(className, qualifiedName, eventName),
			IsLast:        index == len(eventMessages)-1,
		})
	}

	tmplData := EventTemplateData{
		ClassName:      className,
		HeaderFile:     headerFileBase,
		ProtoInclude:   filepath.ToSlash(filepath.Join(_config.Global.DirectoryNames.ProtoDirName, protoRelativeDir, strings.Replace(file.Name(), _config.Global.FileExtensions.Proto, _config.Global.FileExtensions.PbH, 1))),
		EventMessages:  eventHandlers,
		GlobalUserCode: globalCode,
		UserCodeBlocks: userCodeBlocks,
	}

	if err := utils2.RenderTemplateToFile("internal/template/event_handler.h.tmpl", headerFilePath, tmplData); err != nil {
		logger.Global.Error("Failed to generate header file",
			zap.String("header_file", headerFilePath),
			zap.Error(err),
		)
	}
	if err := utils2.RenderTemplateToFile("internal/template/event_handler.cpp.tmpl", cppFilePath, tmplData); err != nil {
		logger.Global.Error("Failed to generate cpp file",
			zap.String("cpp_file", cppFilePath),
			zap.Error(err),
		)
	}
}

func filterProtoFiles(files []os.DirEntry) []os.DirEntry {
	filtered := make([]os.DirEntry, 0, len(files))
	for _, file := range files {
		if utils2.IsProtoFile(file) {
			filtered = append(filtered, file)
		}
	}
	return filtered
}

func filterProtoFilesBySuffix(files []os.DirEntry, suffix string) []os.DirEntry {
	filtered := make([]os.DirEntry, 0, len(files))
	for _, file := range files {
		if !utils2.IsProtoFile(file) {
			continue
		}
		if !strings.HasSuffix(file.Name(), suffix) {
			continue
		}
		filtered = append(filtered, file)
	}
	return filtered
}

func readProtoFiles(relativeDir string) ([]os.DirEntry, error) {
	return os.ReadDir(filepath.Join(_config.Global.Paths.ProtoDir, relativeDir))
}

// GenerateAllEventHandlers generates all event handlers.
func GenerateAllEventHandlers(wg *sync.WaitGroup) {
	logicEventFiles, err := readProtoFiles(_config.Global.PathLists.ProtoDirs.LogicEvent)
	if err != nil {
		logger.Global.Fatal("Failed to read proto directory",
			zap.String("dir", _config.Global.PathLists.ProtoDirs.LogicEvent),
			zap.Error(err),
		)
	}
	logicEventFiles = filterProtoFiles(logicEventFiles)

	for _, file := range logicEventFiles {
		wg.Add(1)
		go generateEventHandlerFiles(wg, file, _config.Global.PathLists.ProtoDirs.LogicEvent, _config.Global.Paths.SceneNodeEventHandlerDirectory)
	}

	kafkaContractFiles := []os.DirEntry{}
	if _config.Global.PathLists.ProtoDirs.ContractsKafka != "" {
		files, readErr := readProtoFiles(_config.Global.PathLists.ProtoDirs.ContractsKafka)
		if readErr != nil {
			logger.Global.Warn("Failed to read Kafka proto directory, skipping Gate event handler generation",
				zap.String("dir", _config.Global.PathLists.ProtoDirs.ContractsKafka),
				zap.Error(readErr),
			)
		} else {
			kafkaContractFiles = filterProtoFilesBySuffix(files, "_event.proto")
			for _, file := range kafkaContractFiles {
				wg.Add(1)
				go generateEventHandlerFiles(wg, file, _config.Global.PathLists.ProtoDirs.ContractsKafka, _config.Global.Paths.GateNodeEventHandlerDirectory)
			}
		}
	}

	wg.Add(1)
	go func() {
		defer wg.Done()
		if err = GenerateEventHandlersTemplate(logicEventFiles, _config.Global.Paths.SceneNodeEventHandlerDirectory); err != nil {
			logger.Global.Fatal("Failed to generate event handler template",
				zap.String("dir", _config.Global.Paths.SceneNodeEventHandlerDirectory),
				zap.Error(err),
			)
			return
		}
		if len(kafkaContractFiles) > 0 {
			if err = GenerateEventHandlersTemplate(kafkaContractFiles, _config.Global.Paths.GateNodeEventHandlerDirectory); err != nil {
				logger.Global.Fatal("Failed to generate Gate event handler template",
					zap.String("dir", _config.Global.Paths.GateNodeEventHandlerDirectory),
					zap.Error(err),
				)
				return
			}
		}
	}()
}

func GenerateEventHandlersTemplate(protoFiles []os.DirEntry, outputDir string) error {
	// Prepare the dynamic data
	var cppIncludeData, registerData, unRegisterData string
	for _, protoFile := range protoFiles {
		if !strings.HasSuffix(protoFile.Name(), _config.Global.FileExtensions.Proto) {
			continue
		}

		cppIncludeData += _config.Global.Naming.IncludeBegin + strings.Replace(filepath.Base(strings.ToLower(protoFile.Name())), _config.Global.FileExtensions.Proto, _config.Global.FileExtensions.HandlerH, 1) + _config.Global.Naming.IncludeEndLine

		className := generateClassNameFromFile(protoFile, _config.Global.Naming.HandlerFile)
		registerData += className + "::Register();\n"
		unRegisterData += className + "::UnRegister();\n"
	}

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

	headerFilePath := filepath.Join(outputDir, _config.Global.Naming.EventHandlerBase+_config.Global.FileExtensions.Header)
	cppFilePath := filepath.Join(outputDir, _config.Global.Naming.EventHandlerBase+_config.Global.FileExtensions.Cpp)
	if err := utils2.RenderTemplateToFile("internal/template/event_handler_total.h.tmpl", headerFilePath, nil); err != nil {
		logger.Global.Error("Failed to generate aggregate header file",
			zap.String("header_file", headerFilePath),
			zap.Error(err),
		)
	}
	if err := utils2.RenderTemplateToFile("internal/template/event_handler_total.cpp.tmpl", cppFilePath, eventCppData); err != nil {
		logger.Global.Error("Failed to generate aggregate cpp file",
			zap.String("cpp_file", cppFilePath),
			zap.Error(err),
		)
	}
	return nil
}

type Config struct {
	ProtoDirs                      []string
	EventProtoDirIndex             int
	ProtoEx                        string
	HandlerHeaderExtension         string
	EventHandlerHeaderFileName     string
	EventHandlerCppFileName        string
	SceneNodeEventHandlerDirectory string
	GateNodeEventHandlerDirectory  string
	IncludeBegin                   string
	IncludeEndLine                 string
	ClassNameSuffix                string
}
