package internal

import (
	"bufio"
	"os"
	"strings"

	"go.uber.org/zap"
	_config "pbgen/internal/config"
	"pbgen/logger"
)

// MethodNameFunc 方法名称生成函数类型
type MethodNameFunc func(info *MethodInfo, funcParam string) string

// CodeSections 代码段解析结果
type CodeSections struct {
	CodeMap   map[string]string
	FirstCode string
}

// ReadCodeSectionsFromFile 从CPP文件中读取代码段
// 使用 methodFunc 动态选择方法名称生成策略
func ReadCodeSectionsFromFile(
	cppFileName string,
	methods *RPCMethods,
	methodFunc MethodNameFunc,
	funcParam string,
) (map[string]string, string, error) {
	codeMap := make(map[string]string)

	fd, err := os.Open(cppFileName)
	if err != nil {
		// 文件不存在是正常情况，返回空结果
		if os.IsNotExist(err) {
			logger.Global.Debug("CPP文件不存在，使用默认代码段",
				zap.String("file_name", cppFileName),
			)
			return buildDefaultCodeMap(methods, methodFunc, funcParam), _config.Global.Naming.YourCodePair, nil
		}
		logger.Global.Error("打开CPP文件失败",
			zap.String("file_name", cppFileName),
			zap.Error(err),
		)
		return codeMap, "", err
	}
	defer fd.Close()

	scanner := bufio.NewScanner(fd)
	parser := newCodeParser(methods, methodFunc, funcParam)
	result := parser.parse(scanner)

	if err := scanner.Err(); err != nil {
		logger.Global.Error("读取CPP文件时出错",
			zap.String("file_name", cppFileName),
			zap.Error(err),
		)
		return result.CodeMap, result.FirstCode, err
	}

	// 填充缺失的方法
	fillMissingMethods(result.CodeMap, methods, methodFunc, funcParam)

	logger.Global.Debug("代码段解析完成",
		zap.String("file_name", cppFileName),
		zap.Int("code_map_size", len(result.CodeMap)),
	)

	return result.CodeMap, result.FirstCode, nil
}

// codeParser 代码段解析器
type codeParser struct {
	methods    *RPCMethods
	methodFunc MethodNameFunc
	funcParam  string

	// 状态
	codeMap       map[string]string
	firstCode     string
	currentCode   string
	currentMethod *MethodInfo
	inFirstCode   bool
	inMethodCode  bool
	isFirstCode   bool
}

func newCodeParser(methods *RPCMethods, methodFunc MethodNameFunc, funcParam string) *codeParser {
	return &codeParser{
		methods:    methods,
		methodFunc: methodFunc,
		funcParam:  funcParam,
		codeMap:    make(map[string]string),
	}
}

func (p *codeParser) parse(scanner *bufio.Scanner) CodeSections {
	for scanner.Scan() {
		line := scanner.Text() + "\n"
		p.processLine(line)
	}

	if p.firstCode == "" {
		p.firstCode = _config.Global.Naming.YourCodePair
	}

	return CodeSections{
		CodeMap:   p.codeMap,
		FirstCode: p.firstCode,
	}
}

func (p *codeParser) processLine(line string) {
	// 处理全局代码段
	if p.handleGlobalCode(line) {
		return
	}

	// 处理方法代码段
	p.handleMethodCode(line)
}

func (p *codeParser) handleGlobalCode(line string) bool {
	// 全局代码段结束
	if p.inFirstCode && strings.Contains(line, _config.Global.Naming.YourCodeEnd) {
		p.firstCode += line
		p.inFirstCode = false
		return true
	}

	// 正在读取全局代码段
	if p.inFirstCode {
		p.firstCode += line
		return true
	}

	// 第一个全局代码段开始
	if !p.isFirstCode && strings.Contains(line, _config.Global.Naming.YourCodeBegin) {
		p.firstCode = line
		p.inFirstCode = true
		p.isFirstCode = true
		return true
	}

	return false
}

func (p *codeParser) handleMethodCode(line string) {
	// 查找方法开始
	if p.currentMethod == nil {
		for _, method := range *p.methods {
			handlerName := p.methodFunc(method, p.funcParam)
			if strings.Contains(line, handlerName) {
				p.currentMethod = method
				break
			}
		}
		return
	}

	// 方法代码段开始
	if strings.Contains(line, _config.Global.Naming.YourCodeBegin) {
		p.inMethodCode = true
		p.currentCode += line
		return
	}

	// 方法代码段结束
	if strings.Contains(line, _config.Global.Naming.YourCodeEnd) {
		p.currentCode += line
		handlerName := p.methodFunc(p.currentMethod, p.funcParam)
		p.codeMap[handlerName] = p.currentCode

		// 重置状态
		p.currentMethod = nil
		p.currentCode = ""
		p.inMethodCode = false
		return
	}

	// 收集方法代码段内容
	if p.inMethodCode {
		p.currentCode += line
	}
}

// buildDefaultCodeMap 构建默认的代码映射
func buildDefaultCodeMap(methods *RPCMethods, methodFunc MethodNameFunc, funcParam string) map[string]string {
	codeMap := make(map[string]string)
	for _, method := range *methods {
		handlerName := methodFunc(method, funcParam)
		codeMap[handlerName] = _config.Global.Naming.YourCodePair
	}
	return codeMap
}

// fillMissingMethods 填充缺失的方法
func fillMissingMethods(codeMap map[string]string, methods *RPCMethods, methodFunc MethodNameFunc, funcParam string) {
	for _, method := range *methods {
		handlerName := methodFunc(method, funcParam)
		if _, exists := codeMap[handlerName]; !exists {
			codeMap[handlerName] = _config.Global.Naming.YourCodePair
		}
	}
}
