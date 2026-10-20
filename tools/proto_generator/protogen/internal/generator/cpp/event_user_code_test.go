package cpp

import (
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"testing"

	_config "protogen/internal/config"
	"protogen/logger"

	"go.uber.org/zap"
)

// extractUserCodeBlocks 改走与 handler 生成器同一套取段规则(internal.ReadCodeSectionsByKeys,grpc-muduo-transport.md §15.17):
// 守护段内与函数体内不认签名、不归任何方法的段整段跳过;结构损坏或带不走的非空段拒绝生成。

func setEventUserCodeNaming(t *testing.T) {
	t.Helper()
	previousNaming, previousLogger := _config.Global.Naming, logger.Global
	t.Cleanup(func() { _config.Global.Naming, logger.Global = previousNaming, previousLogger })
	logger.Global = zap.NewNop()
	_config.Global.Naming.YourCodeBegin = "///<<< BEGIN WRITING YOUR CODE"
	_config.Global.Naming.YourCodeEnd = "///<<< END WRITING YOUR CODE"
	_config.Global.Naming.YourCodePair = _config.Global.Naming.YourCodeBegin + "\n" + _config.Global.Naming.YourCodeEnd
}

func eventSignatures(className string, events ...string) []string {
	signatures := make([]string, 0, len(events))
	for _, event := range events {
		signatures = append(signatures, buildEventHandlerSignature(className, qualifyProtoType("", event), event))
	}
	return signatures
}

func writeEventFixture(t *testing.T, lines ...string) string {
	t.Helper()
	path := filepath.Join(t.TempDir(), "battle_event_handler.cpp")
	if err := os.WriteFile(path, []byte(strings.Join(lines, "\n")+"\n"), 0o600); err != nil {
		t.Fatal(err)
	}
	return path
}

// 修复前每一行都匹配签名、函数体结束也不复位:SettlementEvent 的手写体被 AbortedEvent 的段顶替,AbortedEvent 退回空段
// (函数体里提到别的 handler 签名的注释把解析拉进了那个 handler);缺 FirstCode 时第一个 handler 的段被当成全局段。
func TestExtractUserCodeBlocks_SignatureLiteralsDoNotHijack(t *testing.T) {
	setEventUserCodeNaming(t)
	begin := _config.Global.Naming.YourCodeBegin
	end := _config.Global.Naming.YourCodeEnd
	pair := _config.Global.Naming.YourCodePair
	const className = "BattleEventHandler"
	signatures := eventSignatures(className, "BattleSettlementEvent", "BattleConfirmedEvent", "BattleAbortedEvent")
	settle, confirm, abort := signatures[0], signatures[1], signatures[2]
	section := func(body string) string { return begin + "\n" + body + "\n" + end }
	firstCode := section("// 入口:" + strings.TrimSuffix(abort, "\n"))
	definition := func(signature, body string, after ...string) []string {
		lines := []string{strings.TrimSuffix(signature, "\n"), "{", section(body)}
		return append(append(lines, after...), "}")
	}
	handlers := func() []string {
		var lines []string
		lines = append(lines, definition(settle, "    settle_impl(event);")...)
		lines = append(lines, definition(confirm, "    confirm_impl(event);",
			"    // 结算走 "+strings.TrimSuffix(settle, "\n"))...)
		return append(lines, definition(abort, "    abort_impl(event);")...)
	}
	want := map[string]string{
		settle:  section("    settle_impl(event);"),
		confirm: section("    confirm_impl(event);"),
		abort:   section("    abort_impl(event);"),
	}

	for _, tc := range []struct {
		name       string
		firstCode  []string
		wantGlobal string
	}{
		{name: "函数体里提到别的 handler 签名", firstCode: []string{firstCode}, wantGlobal: firstCode},
		{name: "缺 FirstCode", wantGlobal: pair},
	} {
		t.Run(tc.name, func(t *testing.T) {
			lines := append([]string{"#include \"battle_event_handler.h\"", "#include \"thread_context/ecs_context.h\"", ""}, tc.firstCode...)
			path := writeEventFixture(t, append(lines, handlers()...)...)
			codeMap, global, err := extractUserCodeBlocks(path, signatures)
			if err != nil {
				t.Fatalf("结构完好的文件不应拒绝: %v", err)
			}
			if global != tc.wantGlobal {
				t.Fatalf("全局段不对: got=%q want=%q", global, tc.wantGlobal)
			}
			for signature, code := range want {
				if codeMap[signature] != code {
					t.Fatalf("%q 的手写体被劫持: got=%q want=%q", signature, codeMap[signature], code)
				}
			}
		})
	}
}

// 结构损坏、或非空段在新生成里没有位置时拒绝生成(返回 error,generateEventHandlerFiles 据此 Fatal、不写文件);
// 修复前读失败只打 WARN 照常渲染,带不走的段被静默丢弃。文件不存在(新事件)时全部取默认空段。
func TestExtractUserCodeBlocks_RefusesLossAndDefaultsForNewFile(t *testing.T) {
	setEventUserCodeNaming(t)
	begin := _config.Global.Naming.YourCodeBegin
	end := _config.Global.Naming.YourCodeEnd
	pair := _config.Global.Naming.YourCodePair
	signatures := eventSignatures("BattleEventHandler", "BattleSettlementEvent")
	signature := strings.TrimSuffix(signatures[0], "\n")

	for _, tc := range []struct {
		name  string
		lines []string
	}{
		{name: "handler 体里第一个段之后的非空段", lines: []string{pair, signature, "{", pair, begin, "    extra();", end, "}"}},
		{name: "段没有结束标记", lines: []string{pair, signature, "{", begin, "    settle_impl(event);", "}"}},
		{name: "函数外的非空段", lines: []string{pair, begin, "// 手工插在函数之间", end, signature, "{", pair, "}"}},
	} {
		t.Run(tc.name, func(t *testing.T) {
			path := writeEventFixture(t, tc.lines...)
			if _, _, err := extractUserCodeBlocks(path, signatures); err == nil || !strings.Contains(err.Error(), path) {
				t.Fatalf("必须拒绝并点名文件: err=%v", err)
			}
		})
	}

	codeMap, global, err := extractUserCodeBlocks(filepath.Join(t.TempDir(), "not_created.cpp"), signatures)
	if err != nil || global != pair || codeMap[signatures[0]] != pair {
		t.Fatalf("新文件应全部取默认空段: err=%v global=%q code=%q", err, global, codeMap[signatures[0]])
	}
}

// 工作区里的事件 handler(只读,不写回):签名按文件反推,期望值逐行独立切出 —— 第一个段是全局段,每个签名之后的第一个段归它。
// 改走共用取段规则后,对既有生成物逐字不变,且预检不误报。
func TestExtractUserCodeBlocks_WorkspaceEventHandlersUnchanged(t *testing.T) {
	setEventUserCodeNaming(t)
	root := eventWorkspaceRoot(t)
	signaturePattern := regexp.MustCompile(`^void \w+::\w+\(const .+& event\)$`)
	checked := 0
	for _, glob := range []string{
		"cpp/nodes/scene/handler/event/*_event_handler.cpp",
		"cpp/nodes/gate/handler/event/*_event_handler.cpp",
	} {
		paths, err := filepath.Glob(filepath.Join(root, filepath.FromSlash(glob)))
		if err != nil {
			t.Fatal(err)
		}
		for _, path := range paths {
			data, err := os.ReadFile(path)
			if err != nil {
				t.Fatal(err)
			}
			source := strings.ReplaceAll(string(data), "\r\n", "\n")
			if !strings.Contains(source, _config.Global.Naming.YourCodeBegin) {
				continue
			}
			checked++
			signatures, wantGlobal, wantCode := expectedEventSections(t, path, source, signaturePattern)
			codeMap, global, err := extractUserCodeBlocks(path, signatures)
			if err != nil {
				t.Fatalf("%s 是既有生成物,不得拒绝: %v", path, err)
			}
			if global != wantGlobal {
				t.Fatalf("%s 的全局段变了:\n got=%q\nwant=%q", path, global, wantGlobal)
			}
			for _, signature := range signatures {
				if codeMap[signature] != wantCode[signature] {
					t.Fatalf("%s 的 %q 变了:\n got=%q\nwant=%q", path, signature, codeMap[signature], wantCode[signature])
				}
			}
		}
	}
	if checked == 0 {
		t.Fatal("没有找到工作区的事件 handler,夹具路径可能已变")
	}
}

// expectedEventSections 不复用解析器:守护段外命中 pattern 的行是签名(返回值带行尾换行,与 buildEventHandlerSignature 同形);
// 段以整行开始标记开头、以含结束标记的行结尾;第一个段是全局段,且必须先于第一个签名。段文本不带行尾换行。
func expectedEventSections(t *testing.T, path, source string, pattern *regexp.Regexp) ([]string, string, map[string]string) {
	t.Helper()
	begin := _config.Global.Naming.YourCodeBegin
	end := _config.Global.Naming.YourCodeEnd
	var signatures, current []string
	var global, pending string
	code := make(map[string]string)
	target, inSection, sections := "", false, 0
	for _, line := range strings.Split(source, "\n") {
		trimmed := strings.TrimSpace(line)
		switch {
		case inSection:
			current = append(current, line)
			if strings.Contains(line, end) {
				if sections == 1 && target == "" {
					global = strings.Join(current, "\n")
				} else if target != "" {
					code[target] = strings.Join(current, "\n")
				}
				inSection, current = false, nil
			}
		case trimmed == begin || strings.HasPrefix(trimmed, begin+":"):
			inSection, current = true, []string{line}
			sections++
			target, pending = pending, ""
			if sections == 1 && target != "" {
				t.Fatalf("%s 的第一个段在签名之后,夹具假设不成立", path)
			}
		case pattern.MatchString(line):
			pending = line + "\n"
			signatures = append(signatures, pending)
		}
	}
	if global == "" || len(signatures) == 0 {
		t.Fatalf("%s 里没有全局段或签名,夹具假设不成立", path)
	}
	for _, signature := range signatures {
		if _, exists := code[signature]; !exists {
			t.Fatalf("%s 的 %q 之后没有守护段,夹具假设不成立", path, signature)
		}
	}
	return signatures, global, code
}

// eventWorkspaceRoot 自包目录向上找仓库根;生成器脱离仓库单独运行测试时跳过。
func eventWorkspaceRoot(t *testing.T) string {
	t.Helper()
	dir, err := os.Getwd()
	if err != nil {
		t.Fatal(err)
	}
	marker := filepath.Join("cpp", "nodes", "scene", "handler", "event")
	for {
		if info, err := os.Stat(filepath.Join(dir, marker)); err == nil && info.IsDir() {
			return dir
		}
		parent := filepath.Dir(dir)
		if parent == dir {
			t.Skip("未找到仓库里的 cpp/nodes/scene/handler/event,跳过工作区真实文件回归")
		}
		dir = parent
	}
}
