package internal

import (
	"bufio"
	"fmt"
	"os"
	"regexp"
	"sort"
	"strings"

	_config "protogen/internal/config"
)

// grpcUnnamedSection 是 readNamedGrpcCodeSections 读到的一个无名守护段:Line 是开始标记所在行号(从 1 计),Code 含首尾标记。
type grpcUnnamedSection struct {
	Line int
	Code string
}

// readNamedGrpcCodeSections 读出 path 里的命名守护段(名字 → 含首尾标记的代码)与全部无名守护段(按出现顺序)。
// 命名守护段用于 RPC 的准入段(grpc.<M>.guard)与头文件声明，避免靠段落顺序识别代码。
// 沿用既有 BEGIN/END 标记，仅在 BEGIN 后追加名称。无名段的去留由调用方决定:
//   - cpp:FirstCode 与 Handle* 体由 codeParser 读取,新骨架没有位置的(旧模板段、proto 已删除方法的 Handle* 体等)
//     由 scanGrpcCppSections → checkLegacyGrpcSections 检出;
//   - 头文件:骨架只有三个命名段,非空无名段一律拒绝(checkGrpcHeaderUnnamedSections)。
//
// 结构损坏(嵌套、孤立的结束标记、未闭合、命名段缺名或重名)一律报错。文件不存在视为没有守护段。
func readNamedGrpcCodeSections(path string) (map[string]string, []grpcUnnamedSection, error) {
	sections := make(map[string]string)
	file, err := os.Open(path)
	if os.IsNotExist(err) {
		return sections, nil, nil
	}
	if err != nil {
		return nil, nil, err
	}
	defer file.Close()

	prefix := _config.Global.Naming.YourCodeBegin + ":"
	var unnamed []grpcUnnamedSection
	var name string
	inSection := false
	lineNumber, beginLine := 0, 0
	var code strings.Builder
	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		line := scanner.Text()
		lineNumber++
		trimmed := strings.TrimSpace(line)
		if isCodeSectionBeginMarker(trimmed) {
			if inSection {
				return nil, nil, fmt.Errorf("%s：守护段内不能嵌套守护段", path)
			}
			inSection = true
			beginLine = lineNumber
			if strings.HasPrefix(trimmed, prefix) {
				name = strings.TrimSpace(strings.TrimPrefix(trimmed, prefix))
				if name == "" {
					return nil, nil, fmt.Errorf("%s：命名守护段缺少名称", path)
				}
				if _, exists := sections[name]; exists {
					return nil, nil, fmt.Errorf("%s：守护段 %s 重复", path, name)
				}
			}
			code.WriteString(line + "\n")
			continue
		}
		if trimmed == _config.Global.Naming.YourCodeEnd {
			if !inSection {
				return nil, nil, fmt.Errorf("%s：守护段结束标记没有对应开始标记", path)
			}
			code.WriteString(line + "\n")
			if name != "" {
				sections[name] = strings.TrimRight(code.String(), "\n")
			} else {
				unnamed = append(unnamed, grpcUnnamedSection{Line: beginLine, Code: strings.TrimRight(code.String(), "\n")})
			}
			name = ""
			inSection = false
			code.Reset()
			continue
		}
		if inSection {
			code.WriteString(line + "\n")
		}
	}
	if err := scanner.Err(); err != nil {
		return nil, nil, err
	}
	if inSection {
		return nil, nil, fmt.Errorf("%s：守护段尚未闭合", path)
	}
	return sections, unnamed, nil
}

// allowedNames 照常接受；emptyOnlyNames 是已退役的旧段名(如 grpc.<M>.in_loop / post_dispatch)，
// 只在为空时接受(isEmptyGrpcCodeSection)，由调用方丢弃；其余名字一律拒绝。
func validateNamedGrpcCodeSections(sections map[string]string, allowedNames []string, emptyOnlyNames []string) error {
	allowed := make(map[string]bool, len(allowedNames))
	for _, name := range allowedNames {
		allowed[name] = true
	}
	emptyOnly := make(map[string]bool, len(emptyOnlyNames))
	for _, name := range emptyOnlyNames {
		emptyOnly[name] = true
	}
	var unknown, nonEmptyLegacy []string
	for name, code := range sections {
		switch {
		case allowed[name]:
		case emptyOnly[name]:
			if !isEmptyGrpcCodeSection(code) {
				nonEmptyLegacy = append(nonEmptyLegacy, name)
			}
		default:
			unknown = append(unknown, name)
		}
	}
	if len(nonEmptyLegacy) > 0 {
		sort.Strings(nonEmptyLegacy)
		return fmt.Errorf("旧守护段非空，拒绝覆盖自定义代码(按 docs/design/grpc-muduo-transport.md §15.8.4 手工迁到 grpc.<M>.guard)：%s",
			strings.Join(nonEmptyLegacy, ", "))
	}
	if len(unknown) == 0 {
		return nil
	}
	sort.Strings(unknown)
	return fmt.Errorf("存在未识别的守护段，拒绝覆盖自定义代码：%s", strings.Join(unknown, ", "))
}

func namedGrpcCodeSection(sections map[string]string, name string, indent string) string {
	if code, exists := sections[name]; exists {
		return code
	}
	return indent + _config.Global.Naming.YourCodeBegin + ": " + name + "\n" +
		indent + _config.Global.Naming.YourCodeEnd
}

// grpcImplFunctionPattern 匹配 "<Svc>Impl::<Name>" 限定名(第 1 组服务名、第 2 组函数名):
// 守护段外、顶格且含它的行是一个函数定义的起始行(折行签名的返回类型独占一行时,下一行才是起始行)。
var grpcImplFunctionPattern = regexp.MustCompile(`(\w+)Impl::(~?\w+)`)

// grpcCppSection 是 handler cpp 里的一个守护段(Code 含首尾标记)。Name 为空是无名段;
// Function 是所在函数在 scanGrpcCppSections 返回的签名表里的下标,-1 表示不在任何函数里(如 FirstCode)。
type grpcCppSection struct {
	Name     string
	Code     string
	Function int
}

// scanGrpcCppSections 把 handler cpp 切成「顶格函数的签名」与「守护段」两张表,供旧段检查使用,不依赖当前 proto 的方法列表。
//   - 边界规则与 codeParser 一致:守护段内的自定义代码既不参与边界识别,也不认函数签名;函数体在顶格的 "}" 或 "};" 处结束(isFunctionBodyEnd,行尾空白不计);
//     函数起始行与 codeParser 认查找键的行用同一判定(isCppSignatureLine)。
//   - 签名从起始行累积到以 "{" 结尾的行,行间保留换行,折行签名可以整体匹配;累积途中遇到以 ";" 结尾的行
//     (声明)或函数体结束行(isFunctionBodyEnd)即放弃,遇到新的起始行则从新行重来。
//   - 标记规则与 readNamedGrpcCodeSections 一致(整行恰为开始 / 命名开始 / 结束标记),嵌套、孤立的结束标记、未闭合一律报错。
//
// 文件不存在视为空文件。
func scanGrpcCppSections(path string) ([]string, []grpcCppSection, error) {
	file, err := os.Open(path)
	if os.IsNotExist(err) {
		return nil, nil, nil
	}
	if err != nil {
		return nil, nil, err
	}
	defer file.Close()

	end := _config.Global.Naming.YourCodeEnd
	var signatures []string
	var sections []grpcCppSection
	function := -1 // 正在累积签名或位于其函数体内的函数下标;-1 表示在函数外
	inBody := false
	inSection := false
	var section grpcCppSection
	var code strings.Builder
	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		line := scanner.Text()
		trimmed := strings.TrimSpace(line)
		isBegin := isCodeSectionBeginMarker(trimmed)
		switch {
		case inSection && isBegin:
			return nil, nil, fmt.Errorf("%s：守护段内不能嵌套守护段", path)
		case inSection:
			code.WriteString(line + "\n")
			if trimmed == end {
				section.Code = strings.TrimRight(code.String(), "\n")
				sections = append(sections, section)
				inSection = false
				code.Reset()
			}
		case isBegin:
			inSection = true
			section = grpcCppSection{Name: codeSectionName(trimmed), Function: function}
			code.WriteString(line + "\n")
		case trimmed == end:
			return nil, nil, fmt.Errorf("%s：守护段结束标记没有对应开始标记", path)
		case inBody:
			if isFunctionBodyEnd(line) {
				function, inBody = -1, false
			}
		case isGrpcCppFunctionStart(line):
			signatures = append(signatures, line)
			function, inBody = grpcSignatureProgress(len(signatures)-1, line, trimmed)
		case function >= 0:
			signatures[function] += "\n" + line
			function, inBody = grpcSignatureProgress(function, line, trimmed)
		}
	}
	if err := scanner.Err(); err != nil {
		return nil, nil, err
	}
	if inSection {
		return nil, nil, fmt.Errorf("%s：守护段尚未闭合", path)
	}
	return signatures, sections, nil
}

// grpcSignatureProgress 处理签名里的一行:以 "{" 结尾即进入函数体;以 ";" 结尾(只是声明)或是函数体结束行
// (isFunctionBodyEnd:顶格的 "}" / "};",与 codeParser 同口径)即放弃,回到函数外;否则继续累积。返回新的函数下标与是否已进入函数体。
func grpcSignatureProgress(function int, line, trimmed string) (int, bool) {
	switch {
	case strings.HasSuffix(trimmed, "{"):
		return function, true
	case strings.HasSuffix(trimmed, ";") || isFunctionBodyEnd(line):
		return -1, false
	}
	return function, false
}

// isGrpcCppFunctionStart:签名行(isCppSignatureLine:顶格、不是预处理指令或行注释)且含 "<Svc>Impl::<Name>"。
// 与 codeParser 认查找键同一判定:codeParser 能在其上认出 Handle* 查找键的行,这里必然认作函数起始行。
func isGrpcCppFunctionStart(line string) bool {
	return isCppSignatureLine(line) && grpcImplFunctionPattern.MatchString(line)
}
