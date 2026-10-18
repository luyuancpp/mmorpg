package internal

import (
	"path/filepath"
	"strings"
	"testing"

	_config "protogen/internal/config"
)

// gRPC 生成器的预检(validateGrpcHandlerCppSections)与 codeParser 的取段规则一致性(grpc-muduo-transport.md §15.17.1)。
// 按段形状的扫描(scanGrpcCppSections / checkLegacyGrpcSections)与 codeParser 并不逐条一致;生成时真正取段的是 codeParser,
// 下面三种手改形状扫描全部放行、却会被 codeParser 静默丢掉 —— 修复前照常生成,现在按 codeParser 的 Problems 拒绝;
// 第四种是 codeParser 把命名段当成 Handle* 体取走(不记 Problems),由 refuseNamedSectionsTakenAsUnnamed 拒绝。

// 每种形状各一例:.cpp 生成拒绝(panic,调用方不写文件)且报出 codeParser 的问题,同名 .h 的预检同样拒绝;
// 对照:同一骨架只填 grpc.<M>.guard(codeParser 跳过它是预期)照常生成。
func TestGrpcServiceHandler_RefusesShapesTheScannerAcceptsButCodeParserDrops(t *testing.T) {
	setGrpcPreservationTestConfig(t)
	begin := _config.Global.Naming.YourCodeBegin
	end := _config.Global.Naming.YourCodeEnd
	pair := _config.Global.Naming.YourCodePair
	methods := buildGrpcHandlerTestMethods()
	createKey := GenerateGrpcHandlerNameWrapper(methods[0], "") // SceneNodeGrpcImpl::HandleCreateScene(
	skeleton := GetGrpcServiceHandlerCppStr(filepath.Join(t.TempDir(), "not_created.cpp"), methods, "", "")
	createDefinition := grpcFunctionDefinition(t, skeleton, "void "+createKey)

	for _, tc := range []struct {
		name  string
		build func(t *testing.T) string
		want  string
	}{
		{
			// 文件的第一个段落在一个没有段的现行 Handle* 之后、函数外:扫描器按「文件第一个段」当 FirstCode 放行;
			// codeParser 已进过 HandleCreateScene 的函数体,不再认 FirstCode,整段跳过。
			name: "第一个段落在没有段的现行 Handle* 之后",
			build: func(t *testing.T) string {
				code := replaceGrpcFixtureAfter(t, skeleton, "#include", pair+"\n", "")
				withoutSection := strings.Replace(createDefinition, "{\n"+pair+"\n}", "{\n}", 1)
				return replaceGrpcFixtureOnce(t, code, createDefinition,
					withoutSection+begin+"\n#include \"infra/agones/agones_gameserver_lifecycle.h\"\n"+end+"\n")
			},
			want: "函数外的非空守护段没有去处",
		},
		{
			// 同一 Handle* 定义两次(#if / #else):扫描器认为两处各是自己函数的第一个段;codeParser 后段覆盖前段。
			name: "同一 Handle* 定义两次",
			build: func(t *testing.T) string {
				newer := strings.Replace(createDefinition, pair, begin+"\n    create_with_new_permit(*request, *response);\n"+end, 1)
				older := strings.Replace(createDefinition, pair, begin+"\n    create_legacy(*request, *response);\n"+end, 1)
				return replaceGrpcFixtureOnce(t, skeleton, createDefinition,
					"#if defined(MMORPG_NEW_PERMIT)\n"+newer+"#else\n"+older+"#endif\n")
			},
			want: createKey + " 再次取到守护段",
		},
		{
			// 段内某行夹带结束标记文本:扫描器只认整行结束标记,整段照常;codeParser 按行内含结束标记提前闭合,
			// create_scene_impl 落到段外被丢,真正的结束标记成了孤立标记。
			name: "段内某行夹带结束标记文本",
			build: func(t *testing.T) string {
				return fillGrpcPreservationSection(t, skeleton, "void "+createKey, "",
					"    // 段尾用 "+end+" 收口\n    create_scene_impl(*request, *response);")
			},
			want: "守护段结束标记没有对应开始标记",
		},
		{
			// 最后一个 Handle* 的段被删、结束括号带注释(不是函数体结束行):codeParser 停在它的函数体里,
			// 把其后 CreateScene() 里的 grpc.CreateScene.guard 当成它的体取走 —— 此前没有段可覆盖,Problems 不记;
			// 扫描器只按名字看命名段,同样放行。修复前照常生成,guard 被复制进 HandleAssetAckReceipt 体,产物里 guard 重名。
			name: "命名段被没有段的 Handle* 当成体取走",
			build: func(t *testing.T) string {
				lastKey := GenerateGrpcHandlerNameWrapper(methods[len(methods)-1], "") // SceneNodeGrpcImpl::HandleAssetAckReceipt(
				lastDefinition := grpcFunctionDefinition(t, skeleton, "void "+lastKey)
				withoutSection := replaceGrpcFixtureOnce(t, lastDefinition, "{\n"+pair+"\n}\n", "{\n}  // 守护段被手工删掉\n")
				code := replaceGrpcFixtureOnce(t, skeleton, lastDefinition, withoutSection)
				return fillGrpcPreservationSection(t, code, "", "grpc.CreateScene.guard",
					"    if (!ready_impl())\n    {\n        call->Reject(muduo_grpc::Status(muduo_grpc::StatusCode::kUnavailable, \"not_ready\"));\n        return;\n    }")
			},
			want: "HandleAssetAckReceipt 体:grpc.CreateScene.guard",
		},
	} {
		t.Run(tc.name, func(t *testing.T) {
			dir := t.TempDir()
			headerPath := filepath.Join(dir, "scene_node_service.h")
			cppPath := filepath.Join(dir, "scene_node_service.cpp")
			code := tc.build(t)
			writeGrpcPreservationFile(t, cppPath, code)
			if err := checkLegacyGrpcSections(cppPath, methods); err != nil {
				t.Fatalf("夹具假设不成立:按段形状的扫描应放行这种形状: %v", err)
			}

			message := recoverGrpcGenerationPanic(t, func() { GetGrpcServiceHandlerCppStr(cppPath, methods, "", "") })
			for _, want := range []string{cppPath, tc.want, "拒绝生成"} {
				if !strings.Contains(message, want) {
					t.Fatalf(".cpp 拒绝信息缺少 %q:\n%s", want, message)
				}
			}
			if _, err := GetGrpcServiceHandlerHeadFromFileStr(headerPath, methods); err == nil || !strings.Contains(err.Error(), tc.want) {
				t.Fatalf(".h 预检必须同样拒绝: err=%v", err)
			}
			assertGrpcFileContent(t, cppPath, code)
		})
	}

	t.Run("对照:只填 guard 段照常生成", func(t *testing.T) {
		cppPath := filepath.Join(t.TempDir(), "scene_node_service.cpp")
		guard := "    if (!ready_impl())\n    {\n        call->Reject(muduo_grpc::Status(muduo_grpc::StatusCode::kUnavailable, \"not_ready\"));\n        return;\n    }"
		writeGrpcPreservationFile(t, cppPath, fillGrpcPreservationSection(t, skeleton, "", "grpc.CreateScene.guard", guard))
		assertGrpcPreservedOnce(t, GetGrpcServiceHandlerCppStr(cppPath, methods, "", ""), guard, "guard 段")
	})
}

// 扫描器与 codeParser 同批放宽函数体结束行:顶格的 "}" 或 "};",行尾空白不计(isFunctionBodyEnd);缩进的 "}" 仍是嵌套块。
// 修复前扫描器只认整行恰为 "}":"} " 之后的 Handle* 签名落在「函数体内」不被认出,它的段被归到上一个函数的第二个段,
// checkLegacyGrpcSections 把它误报成带不走的 Handle* 体。codeParser 侧见 TestReadCodeSections_FunctionBodyEndsOnTopLevelBraceVariants。
func TestScanGrpcCppSections_FunctionBodyEndsOnTopLevelBraceVariants(t *testing.T) {
	setParserTestNaming(t)
	begin := _config.Global.Naming.YourCodeBegin
	end := _config.Global.Naming.YourCodeEnd
	path := writeParserTempFile(t, strings.Join([]string{
		"#include \"battle_node.h\"",
		begin,
		end,
		"void BattleNodeImpl::HandleCreateBattle(const ::CreateBattleRequest* request,",
		"    ::CreateBattleResponse* response)",
		"{",
		"    if (request == nullptr)",
		"    {",
		"    }",
		begin,
		"    create_impl(*request, *response);",
		end,
		"} ",
		"void BattleNodeImpl::HandleDestroyBattle(const ::DestroyBattleRequest* request)",
		"{",
		begin,
		"    destroy_impl(*request);",
		end,
		"};",
		"void BattleNodeImpl::HandleAddObserver(const ::AddObserverRequest* request,",
		"    ::AddObserverResponse* response)",
		"{",
		begin,
		"    add_observer_impl(*request, *response);",
		end,
		"}\t",
	}, "\n")+"\n")

	signatures, sections, err := scanGrpcCppSections(path)
	if err != nil {
		t.Fatalf("扫描失败: %v", err)
	}
	if len(signatures) != 3 {
		t.Fatalf("应切出三个 Handle* 函数: %q", signatures)
	}
	wantFunctions := []int{-1, 0, 1, 2} // FirstCode 在函数外,其后每个 Handle* 恰好一个段
	if len(sections) != len(wantFunctions) {
		t.Fatalf("守护段数量不对: got=%d want=%d", len(sections), len(wantFunctions))
	}
	for i, function := range wantFunctions {
		if sections[i].Function != function {
			t.Fatalf("第 %d 个段归属不对: got=%d want=%d", i, sections[i].Function, function)
		}
	}
	if err := checkLegacyGrpcSections(path, buildBattleGrpcPreservationMethods()); err != nil {
		t.Fatalf("每个 Handle* 体都是自己函数的第一个段,不应误报: %v", err)
	}
}

// grpcFunctionDefinition 截出 code 里以 signature 开头、到顶格结束括号为止的函数定义(含结尾换行)。
func grpcFunctionDefinition(t *testing.T, code, signature string) string {
	t.Helper()
	start := strings.Index(code, signature)
	if start < 0 {
		t.Fatalf("夹具缺少函数签名 %q", signature)
	}
	end := strings.Index(code[start:], "\n}\n")
	if end < 0 {
		t.Fatalf("%q 没有顶格的结束括号", signature)
	}
	return code[start : start+end+3]
}

// replaceGrpcFixtureAfter 把 anchor 之后第一次出现的 from 换成 to;找不到即失败,防止用例空转。
func replaceGrpcFixtureAfter(t *testing.T, code, anchor, from, to string) string {
	t.Helper()
	at := strings.Index(code, anchor)
	if at < 0 {
		t.Fatalf("夹具缺少 %q", anchor)
	}
	offset := strings.Index(code[at:], from)
	if offset < 0 {
		t.Fatalf("夹具里 %q 之后没有 %q", anchor, from)
	}
	position := at + offset
	return code[:position] + to + code[position+len(from):]
}
