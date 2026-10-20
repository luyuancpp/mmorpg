package internal

import (
	"bufio"
	"fmt"
	"os"
	"regexp"
	"strings"

	_config "protogen/internal/config"
	"protogen/logger"

	"go.uber.org/zap"
)

// MethodNameFunc generates handler names from method info.
type MethodNameFunc func(info *MethodInfo, funcParam string) string

// CodeSections holds parsed code sections from a CPP file.
type CodeSections struct {
	// CodeMap:查找键 → 该方法的守护段(含首尾标记)。没取到段的键填默认空段。
	CodeMap   map[string]string
	FirstCode string
	// Problems:结构损坏(未闭合 / 嵌套 / 孤立的结束标记)与「读到了、但新生成里没有位置」的非空段,每条一句、带行号与类别。
	// 宽松入口(ReadCodeSectionsFromFile)忽略它;生成器在渲染前拒绝:handler / 玩家 handler / 事件 handler 生成器经
	// RefuseLostCodeSections 全部拒绝,gRPC 生成器(validateGrpcHandlerCppSections)只放过它另行按名字读走的当前方法 grpc.<M>.guard 段。
	Problems []CodeSectionProblem
}

// CodeSectionProblemKind 是 Problems 的成因类别,供调用方区分「预期被跳过」与「会丢代码 / 结构损坏」。
type CodeSectionProblemKind uint8

const (
	// CodeSectionBroken:结构损坏 —— 未闭合、嵌套(孤立的开始标记)、孤立的结束标记(含段内某行夹带结束标记文本、
	// 被按行内含结束标记提前闭合之后剩下的那个结束标记)。
	CodeSectionBroken CodeSectionProblemKind = iota
	// CodeSectionUnplaced:非空段在新生成里没有原样的位置 —— 被整段跳过(函数外、签名不对应当前方法的函数里、
	// 方法体里第一个段之后),或落在这样的函数里却被当成 FirstCode 搬到文件开头。
	CodeSectionUnplaced
	// CodeSectionOverwritten:同一方法再次取到段(签名重复出现,如 #if / #else 两份定义),先取到的非空段被覆盖。
	CodeSectionOverwritten
)

// CodeSectionProblem 是 Problems 的一条。
type CodeSectionProblem struct {
	Kind CodeSectionProblemKind
	// Line:问题所在行(从 1 计),通常是开始标记所在行。
	Line int
	// SectionName:被跳过的是命名段时为它的名字(开始标记 ":" 之后去掉首尾空白),其余为空。
	SectionName string
	// Message:完整的一句,以「第 N 行：」开头。
	Message string
}

func (problem CodeSectionProblem) String() string { return problem.Message }

// ReadCodeSectionsFromFile parses user-written code sections between markers in a CPP file.
// 宽松入口:只返回 I/O 错误,结构问题不报错(见 CodeSections.Problems)。生成器不走这里:handler / 玩家 handler 生成器用
// readCheckedCodeSections,gRPC 生成器用 validateGrpcHandlerCppSections 交回的同一次解析结果。
func ReadCodeSectionsFromFile(
	cppFileName string,
	methods *RPCMethods,
	methodFunc MethodNameFunc,
	funcParam string,
) (map[string]string, string, error) {
	sections, err := ReadCodeSectionsByKeys(cppFileName, methodLookupKeys(methods, methodFunc, funcParam))
	return sections.CodeMap, sections.FirstCode, err
}

// readCheckedCodeSections 是 handler / 玩家 handler 生成器的取段入口:与 ReadCodeSectionsFromFile 同一次解析,
// 另把 Problems 报成 error —— 与 gRPC 生成器的 validateGrpcHandlerCppSections 同口径,宁可拒绝生成也不吞代码。
func readCheckedCodeSections(
	cppFileName string,
	methods *RPCMethods,
	methodFunc MethodNameFunc,
	funcParam string,
) (map[string]string, string, error) {
	sections, err := ReadCodeSectionsByKeys(cppFileName, methodLookupKeys(methods, methodFunc, funcParam))
	if err != nil {
		return nil, "", err
	}
	if err := RefuseLostCodeSections(cppFileName, sections); err != nil {
		return nil, "", err
	}
	return sections.CodeMap, sections.FirstCode, nil
}

// ReadCodeSectionsByKeys 按查找键取段:keys 按方法顺序排列,签名起始行同时含多个键时取靠前的。
// 文件不存在时 FirstCode 与每个键都取默认空段。CodeMap 以键本身为键;读失败时返回已读到的部分与 error。
func ReadCodeSectionsByKeys(cppFileName string, keys []string) (CodeSections, error) {
	fd, err := os.Open(cppFileName)
	if err != nil {
		if os.IsNotExist(err) {
			return CodeSections{CodeMap: defaultCodeMap(keys), FirstCode: _config.Global.Naming.YourCodePair}, nil
		}
		logger.Global.Error("Failed to open CPP file",
			zap.String("file_name", cppFileName),
			zap.Error(err),
		)
		return CodeSections{CodeMap: make(map[string]string)}, err
	}
	defer fd.Close()

	scanner := bufio.NewScanner(fd)
	result := newCodeParser(keys).parse(scanner)

	if err := scanner.Err(); err != nil {
		logger.Global.Error("Error reading CPP file",
			zap.String("file_name", cppFileName),
			zap.Error(err),
		)
		return result, err
	}

	// Fill missing methods with default code.
	for _, key := range keys {
		if _, exists := result.CodeMap[key]; !exists {
			result.CodeMap[key] = _config.Global.Naming.YourCodePair
		}
	}
	return result, nil
}

// RefuseLostCodeSections 把 sections.Problems 合成一条拒绝生成的 error;没有问题时返回 nil。
func RefuseLostCodeSections(cppFileName string, sections CodeSections) error {
	if len(sections.Problems) == 0 {
		return nil
	}
	messages := make([]string, 0, len(sections.Problems))
	for _, problem := range sections.Problems {
		messages = append(messages, problem.Message)
	}
	return fmt.Errorf("%s：%d 处守护段结构损坏或非空却在新生成里没有位置，拒绝生成(宁可拒绝也不吞代码)："+
		"请先补齐 / 删掉孤立的标记(结束标记须独占一行，段内不要夹带它的文本)，或把代码移进对应方法的第一个守护段 / "+
		"文件开头的 FirstCode(或删除)，再生成：\n  %s",
		cppFileName, len(sections.Problems), strings.Join(messages, "\n  "))
}

func methodLookupKeys(methods *RPCMethods, methodFunc MethodNameFunc, funcParam string) []string {
	keys := make([]string, 0, len(*methods))
	for _, method := range *methods {
		keys = append(keys, methodFunc(method, funcParam))
	}
	return keys
}

// codeParser is a stateful parser for extracting user code sections.
//
// 所有 handler 生成器(handler_gen / player_service_gen / grpc_handler_gen)与事件 handler 生成器(event.go)共用这一套取段规则:
//   - FirstCode:文件里的第一个守护段,且出现在任何当前方法进入函数体、或签名已命中之前(被放弃的前置声明不算)。
//     它若落在签名不对应当前方法的函数体内(已删方法 / 手写辅助函数),照旧按 FirstCode 取走,但非空时记进 Problems
//     (CodeSectionUnplaced):重生成会把它从那个函数里搬到文件开头。
//   - 函数边界与 grpc 的 scanGrpcCppSections 同一套签名累积规则:守护段外的函数起始行(isCppFunctionStart:签名行且含
//     "<类>::<名>(")开始一个签名;签名累积到以 "{" 结尾的行才进入函数体,累积途中遇到以 ";" 结尾的行(只是声明)即放弃,
//     遇到新的起始行则从新行重来;函数体在顶格的 "}" 或 "};"(行尾空白不计,isFunctionBodyEnd)处结束。
//   - 方法段:签名起始行逐字含某方法的查找键即归该方法,其后第一个守护段就是它的代码。函数体内不认起始行、不认查找键 ——
//     函数体里顶格的 "<类>::Handle<M>(" 调用、或被放弃的前置声明,都不得把其后的段劫持给别的方法。
//     方法段与 FirstCode 按「行内含开始标记」起段、不区分命名段:等段的方法(或 FirstCode 的位置)之后先遇到命名段时,
//     命名段被当成它的代码取走、不记 Problems(如 Handle* 自己没有段、结束括号又不是函数体结束行);gRPC 生成器另行拒绝
//     (refuseNamedSectionsTakenAsUnnamed),其余生成器的骨架里没有命名段。
//   - 其余守护段(不归任何方法:grpc.<M>.guard、方法体里第一个段之后的段、签名不对应当前方法的函数里的段、FirstCode 之后
//     函数外的段)整段跳过、不进 codeMap。其中非空的记进 Problems(CodeSectionUnplaced,命名段带名字):生成器据此拒绝生成,
//     gRPC 生成器只放过它按名字另行读走的当前方法 grpc.<M>.guard。
//   - 任何守护段内部(开始标记之后、结束标记之前)都不做查找键匹配,也不参与函数边界识别:段内是自定义代码,
//     出现某方法的查找键(带类名限定的调用、注释)不得把解析拉进那个方法 —— 否则随后的结束标记会把该方法在 codeMap 里
//     已取到的代码覆盖成只剩结束标记,手写体被静默丢掉。
//   - 跳过段里又出现整行开始标记(打开它的是没有结束标记的孤立标记,结构已损坏)时撤销跳过、按段外规则回放(handleSkippedSection),
//     孤立标记不得吞掉其后方法的签名与守护段。结构完好的文件里段不嵌套,回放不会发生。
//
// 结构损坏一律记进 Problems,但宽松解析照旧给出尽力而为的结果(既有调用方与回归用例依赖它)。
type codeParser struct {
	keys []string

	codeMap     map[string]string
	firstCode   string
	currentCode string
	// currentKey:签名起始行命中、还没取到段的方法查找键;为空表示当前没有等段的方法。
	currentKey   string
	inFirstCode  bool
	inMethodCode bool
	// inSkippedSection:位于既不是 FirstCode、也不归任何方法的守护段内,段内各行原样跳过,直到结束标记。
	inSkippedSection bool
	// skippedBegin / skippedLines:当前跳过段的开始标记行与其后读到的行。段闭合时据此判断是否非空;
	// 段内又出现开始标记时回放 skippedLines(handleSkippedSection)。段闭合即清空。
	skippedBegin numberedLine
	skippedLines []numberedLine
	// skippedName:当前跳过段是命名段时的名字,无名段为空(随 Problems 交出,gRPC 生成器据此放过 grpc.<M>.guard)。
	skippedName   string
	isFirstCode   bool
	hasSeenMethod bool
	// firstCodeFunction:FirstCode 开始时所在函数的 "<类>::<名>"(签名不对应当前方法的函数;不在函数里为空),闭合时据此记 Problems。
	firstCodeFunction string

	// 函数边界(只在守护段外跟踪)。inSignature:正在累积签名;inBody:位于函数体内。
	inSignature bool
	inBody      bool
	// functionName / functionKey:当前函数(累积中或函数体内)的 "<类>::<名>" 与签名起始行命中的查找键(未命中为空),
	// 只用于给 Problems 定位;函数外都为空。
	functionName string
	functionKey  string

	// lineNumber:正在处理的行号(从 1 计;回放时是被回放行的原行号)。sectionBeginLine:当前方法段 / FirstCode 的开始标记行号。
	lineNumber       int
	sectionBeginLine int
	problems         []CodeSectionProblem
}

// numberedLine 是带原行号的一行(含行尾换行)。
type numberedLine struct {
	number int
	text   string
}

func newCodeParser(keys []string) *codeParser {
	return &codeParser{
		keys:    keys,
		codeMap: make(map[string]string),
	}
}

func (p *codeParser) parse(scanner *bufio.Scanner) CodeSections {
	for scanner.Scan() {
		p.lineNumber++
		line := scanner.Text() + "\n"
		p.processLine(line)
	}

	switch {
	case p.inFirstCode || p.inMethodCode:
		p.problemf(CodeSectionBroken, p.sectionBeginLine, "守护段尚未闭合")
	case p.inSkippedSection:
		p.problemf(CodeSectionBroken, p.skippedBegin.number, "守护段尚未闭合")
	}

	if p.firstCode == "" {
		p.firstCode = _config.Global.Naming.YourCodePair
	}

	return CodeSections{
		CodeMap:   p.codeMap,
		FirstCode: p.firstCode,
		Problems:  p.problems,
	}
}

func (p *codeParser) problemf(kind CodeSectionProblemKind, line int, format string, args ...any) {
	p.sectionProblemf(kind, line, "", format, args...)
}

// sectionProblemf:sectionName 为被跳过的命名段的名字(无名段与非跳过段的问题传空)。
func (p *codeParser) sectionProblemf(kind CodeSectionProblemKind, line int, sectionName, format string, args ...any) {
	p.problems = append(p.problems, CodeSectionProblem{
		Kind:        kind,
		Line:        line,
		SectionName: sectionName,
		Message:     fmt.Sprintf("第 %d 行：", line) + fmt.Sprintf(format, args...),
	})
}

func (p *codeParser) processLine(line string) {
	if p.handleGlobalCode(line) {
		return
	}
	p.handleMethodCode(line)
}

func (p *codeParser) handleGlobalCode(line string) bool {
	if p.inFirstCode && strings.Contains(line, _config.Global.Naming.YourCodeEnd) {
		p.firstCode += line
		p.inFirstCode = false
		if p.firstCodeFunction != "" && !isEmptyGrpcCodeSection(p.firstCode) {
			// 缺 FirstCode 的文件里,第一个段落在签名不对应当前方法的函数体内:宽松解析照旧把它当 FirstCode,
			// 但重生成会把它从那个函数里搬到文件开头(函数本身随之消失),语义已经变了,生成器应拒绝。
			p.problemf(CodeSectionUnplaced, p.sectionBeginLine,
				"%s 里的守护段是文件的第一个段,会被当成 FirstCode 搬到文件开头(该函数的签名不对应当前任何方法)", p.firstCodeFunction)
		}
		return true
	}
	if p.inFirstCode {
		if isCodeSectionBeginMarker(strings.TrimSpace(line)) {
			p.problemf(CodeSectionBroken, p.lineNumber, "守护段内不能嵌套守护段(第 %d 行的开始标记没有结束标记)", p.sectionBeginLine)
		}
		p.firstCode += line
		return true
	}
	// Global preserved-code block is only valid before any method signature appears.
	// 签名已命中、还没进入函数体的方法(currentKey)同样算出现过:它的段不能被当成 FirstCode。
	// 被放弃的前置声明不算(hasSeenMethod 只在进入函数体或取段时置位),不会让真正的 FirstCode 落空。
	if !p.isFirstCode && !p.hasSeenMethod && p.currentKey == "" &&
		strings.Contains(line, _config.Global.Naming.YourCodeBegin) {
		p.firstCode = line
		p.inFirstCode = true
		p.isFirstCode = true
		p.sectionBeginLine = p.lineNumber
		if p.inSignature || p.inBody {
			p.firstCodeFunction = p.functionName
		}
		return true
	}
	return false
}

func (p *codeParser) handleMethodCode(line string) {
	if p.inSkippedSection {
		p.handleSkippedSection(line)
		return
	}
	trimmed := strings.TrimSpace(line)
	if p.inMethodCode {
		if isCodeSectionBeginMarker(trimmed) {
			p.problemf(CodeSectionBroken, p.lineNumber, "守护段内不能嵌套守护段(第 %d 行的开始标记没有结束标记)", p.sectionBeginLine)
		}
		p.currentCode += line
		if strings.Contains(line, _config.Global.Naming.YourCodeEnd) {
			p.finishMethodCode()
		}
		return
	}

	// 以下都在守护段之外。
	bare := strings.TrimRight(line, "\r\n")
	// 生成函数的结束大括号固定顶格:函数体在此结束,旧包装函数没有守护段时也在此结束匹配,
	// 否则下一个函数的守护段会被归给当前方法。
	if isFunctionBodyEnd(bare) {
		p.leaveFunction()
		return
	}
	if p.currentKey != "" && strings.Contains(line, _config.Global.Naming.YourCodeBegin) {
		p.inMethodCode = true
		p.hasSeenMethod = true
		p.sectionBeginLine = p.lineNumber
		p.currentCode = line
		return
	}
	// 不归任何方法的段按整行标记识别(isCodeSectionBeginMarker,与 grpc 的 scanGrpcCppSections 同一规则):
	// 段外的注释里顺带提到开始标记文本,不会被当成段的开头,把其后的函数签名一并跳过。
	if isCodeSectionBeginMarker(trimmed) {
		p.inSkippedSection = true
		p.skippedBegin = numberedLine{number: p.lineNumber, text: line}
		p.skippedName = codeSectionName(trimmed)
		return
	}
	if strings.Contains(line, _config.Global.Naming.YourCodeEnd) {
		// 孤立的结束标记:只记问题、不当成任何段,等段的方法照旧等它的段。
		p.problemf(CodeSectionBroken, p.lineNumber, "守护段结束标记没有对应开始标记")
		return
	}
	p.trackFunctionBoundary(bare, trimmed)
}

// finishMethodCode:当前方法段闭合,归入 codeMap。同一方法第二次取到段(签名重复出现)时,先取到的非空段会被覆盖,记进 Problems。
func (p *codeParser) finishMethodCode() {
	if previous, exists := p.codeMap[p.currentKey]; exists && !isEmptyGrpcCodeSection(previous) {
		p.problemf(CodeSectionOverwritten, p.sectionBeginLine, "%s 再次取到守护段,先取到的非空段会被覆盖(签名重复出现)", p.currentKey)
	}
	p.codeMap[p.currentKey] = p.currentCode
	p.currentKey = ""
	p.currentCode = ""
	p.inMethodCode = false
}

// trackFunctionBoundary 处理守护段外、不是函数体结束行(isFunctionBodyEnd)、也不是标记的一行(规则见 codeParser;与 grpcSignatureProgress 同口径)。
func (p *codeParser) trackFunctionBoundary(bare, trimmed string) {
	switch {
	case p.inBody:
		return // 函数体内不认起始行、不认查找键
	case isCppFunctionStart(bare):
		p.startFunction(bare)
	case !p.inSignature:
		return
	}
	switch {
	case strings.HasSuffix(trimmed, "{"):
		p.inSignature = false
		p.inBody = true
		if p.currentKey != "" {
			p.hasSeenMethod = true
		}
	case strings.HasSuffix(trimmed, ";"):
		// 只是声明(前置声明、函数体外的调用):放弃,不进入任何方法。
		p.leaveFunction()
	}
}

// startFunction:守护段外的函数起始行开始一个新签名,按方法表顺序取第一个逐字命中的查找键(没有则不归任何方法)。
// 正在累积的签名被新起始行取代。
func (p *codeParser) startFunction(line string) {
	p.inSignature = true
	p.functionKey = ""
	for _, key := range p.keys {
		if strings.Contains(line, key) {
			p.functionKey = key
			break
		}
	}
	p.currentKey = p.functionKey
	p.functionName = line
	if match := cppFunctionNamePattern.FindStringSubmatch(line); match != nil {
		p.functionName = match[1] + "::" + match[2]
	}
}

func (p *codeParser) leaveFunction() {
	p.currentKey = ""
	p.inSignature = false
	p.inBody = false
	p.functionName = ""
	p.functionKey = ""
}

// handleSkippedSection 处理跳过段内的一行:与方法段、FirstCode 同口径,行内含结束标记即闭合,段内各行不进 codeMap;
// 闭合时非空的段记进 Problems(它在新生成里没有位置)。
// 段内又出现整行开始标记,说明打开本段的开始标记没有配对的结束标记(手工编辑留下的孤立标记,结构已损坏):记进 Problems、撤销本段,
// 把孤立标记之后读到的行连同本行按段外规则重新处理。否则孤立标记会一路跳到下一个方法段的结束标记,把其间方法的签名
// 连同它的守护段一起吞掉,该方法静默退回默认空体。回放的行里既没有整行开始标记也没有结束标记(遇到即已回放或闭合),
// 回放不会重新进入跳过段、不会递归。生成器在 Problems 非空时拒绝生成,回放只服务宽松解析。
func (p *codeParser) handleSkippedSection(line string) {
	if strings.Contains(line, _config.Global.Naming.YourCodeEnd) {
		p.reportSkippedSection(line)
		p.inSkippedSection = false
		p.skippedLines = nil
		return
	}
	if !isCodeSectionBeginMarker(strings.TrimSpace(line)) {
		p.skippedLines = append(p.skippedLines, numberedLine{number: p.lineNumber, text: line})
		return
	}
	p.problemf(CodeSectionBroken, p.lineNumber, "守护段内不能嵌套守护段(第 %d 行的开始标记没有结束标记)", p.skippedBegin.number)
	replay := p.skippedLines
	current := p.lineNumber
	p.inSkippedSection = false
	p.skippedLines = nil
	for _, skipped := range replay {
		p.lineNumber = skipped.number
		p.processLine(skipped.text)
	}
	p.lineNumber = current
	p.processLine(line)
}

// reportSkippedSection:跳过段以 endLine 闭合;非空就记进 Problems,按所在位置说明它为什么没有去处。
func (p *codeParser) reportSkippedSection(endLine string) {
	var code strings.Builder
	code.WriteString(p.skippedBegin.text)
	for _, skipped := range p.skippedLines {
		code.WriteString(skipped.text)
	}
	code.WriteString(endLine)
	if isEmptyGrpcCodeSection(code.String()) {
		return
	}
	label := "非空守护段"
	if p.skippedName != "" {
		label = "非空守护段(" + p.skippedName + ")"
	}
	switch {
	case !p.inSignature && !p.inBody:
		p.sectionProblemf(CodeSectionUnplaced, p.skippedBegin.number, p.skippedName,
			"函数外的%s没有去处(只有文件里、任何方法之前的第一个段是 FirstCode)", label)
	case p.functionKey == "":
		p.sectionProblemf(CodeSectionUnplaced, p.skippedBegin.number, p.skippedName,
			"%s 的签名不对应当前任何方法(proto 已删除的方法或手写函数),其中的%s没有去处", p.functionName, label)
	default:
		p.sectionProblemf(CodeSectionUnplaced, p.skippedBegin.number, p.skippedName,
			"%s 体里第一个守护段之后的%s没有去处", p.functionName, label)
	}
}

// codeSectionName:整行开始标记(isCodeSectionBeginMarker 成立的 trimmed)里的段名,无名段为空。与 scanGrpcCppSections 同一取法。
func codeSectionName(trimmed string) string {
	return strings.TrimSpace(strings.TrimPrefix(strings.TrimPrefix(trimmed, _config.Global.Naming.YourCodeBegin), ":"))
}

// isFunctionBodyEnd:顶格的 "}" 或 "};"(行尾空白不计)结束函数体;codeParser 与 scanGrpcCppSections 共用。
// 行首不许有空白:函数体里缩进的 "}" 是嵌套块的结束,不是函数的;生成的函数结束括号一律顶格。
func isFunctionBodyEnd(line string) bool {
	trimmed := strings.TrimRight(line, " \t\r\n")
	return trimmed == "}" || trimmed == "};"
}

// cppFunctionNamePattern 匹配函数定义里的限定名 "<类>::<名>("(第 1 组类名、第 2 组函数名),"(" 前允许空白。
// 三个 handler 生成器与事件 handler 的查找键都是这个形状的子串,所以能认出查找键的行必然是函数起始行。
var cppFunctionNamePattern = regexp.MustCompile(`(\w+)::(~?\w+)\s*\(`)

// isCppFunctionStart:签名行(isCppSignatureLine)且含 "<类>::<名>("。与 grpc 的 isGrpcCppFunctionStart 相近但不是其超集:
// 那边要求 "<Svc>Impl::<名>"、不要求其后紧跟 "(",只服务 grpc 骨架;能认出 Handle* 查找键的行两边都认作起始行。
// 不含限定名的顶格行(返回类型独占一行、"{"、namespace 等)不开始签名,只在累积途中作为续行。
func isCppFunctionStart(line string) bool {
	return isCppSignatureLine(line) && cppFunctionNamePattern.MatchString(line)
}

// isCppSignatureLine 判断守护段外的一行(不含行尾换行)能否是函数定义的签名行:顶格(不以空白开头)、
// 不是预处理指令、也不是行注释。生成的函数签名一律顶格;只有这样的行才可能开始签名,
// 缩进的函数体语句与注释里出现查找键都不算进入方法。
// grpc 的 scanGrpcCppSections 用同一判定认函数起始行(isGrpcCppFunctionStart),两边对「哪一行是签名」同一口径。
func isCppSignatureLine(line string) bool {
	if line == "" || line[0] == ' ' || line[0] == '\t' {
		return false
	}
	return !strings.HasPrefix(line, "#") && !strings.HasPrefix(line, "//")
}

// isCodeSectionBeginMarker:去掉首尾空白后整行恰为开始标记,或是命名开始标记(开始标记后紧跟 ":")。
// grpc 的 readNamedGrpcCodeSections / scanGrpcCppSections / isEmptyGrpcCodeSection 与 codeParser 的跳过段共用这一规则。
func isCodeSectionBeginMarker(trimmed string) bool {
	begin := _config.Global.Naming.YourCodeBegin
	return trimmed == begin || strings.HasPrefix(trimmed, begin+":")
}

func defaultCodeMap(keys []string) map[string]string {
	codeMap := make(map[string]string, len(keys))
	for _, key := range keys {
		codeMap[key] = _config.Global.Naming.YourCodePair
	}
	return codeMap
}
