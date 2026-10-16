package internal

import (
	"os"
	"path/filepath"
	"strings"
	"testing"

	_config "protogen/internal/config"

	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"
)

func TestGrpcServiceHandler_NewBattleSkeleton(t *testing.T) {
	setGrpcPreservationTestConfig(t)
	methods := buildBattleGrpcPreservationMethods()
	dst := filepath.Join(t.TempDir(), "battle_node.h")

	header, err := GetGrpcServiceHandlerHeadFromFileStr(dst, methods)
	if err != nil {
		t.Fatalf("新建头文件失败: %v", err)
	}
	for _, name := range []string{"grpc.header.includes", "grpc.header.public", "grpc.header.private"} {
		assertGrpcPreservedOnce(t, header, grpcPreservationBegin(name), "头文件守护段")
	}
	cpp := GetGrpcServiceHandlerCppStr(dst+".cpp", methods, "", "")
	if strings.Contains(cpp, _config.Global.Naming.YourCodeEnd+"}") {
		t.Fatal("结束守护标记不能吞掉函数结束括号")
	}
	writeGrpcPreservationFile(t, dst+".cpp", cpp)
	if next := GetGrpcServiceHandlerCppStr(dst+".cpp", methods, "", ""); next != cpp {
		t.Fatal("首次生成的空骨架必须可以立即重复生成且字节不变")
	}
	for _, method := range methods {
		name := method.Method()
		assertGrpcPreservedOnce(t, header, "grpc::Status "+name+"(", "RPC 声明")
		assertGrpcPreservedOnce(t, cpp, "grpc::Status BattleNodeImpl::"+name+"(", "RPC 包装函数")
		assertGrpcPreservedOnce(t, cpp, "void BattleNodeImpl::Handle"+name+"(", "业务委托函数")
		for _, phase := range []string{"in_loop", "post_dispatch"} {
			assertGrpcPreservedOnce(t, cpp, grpcPreservationBegin("grpc."+name+"."+phase), "RPC 命名守护段")
		}
	}
	if got := strings.Count(cpp, "std::promise<grpc::Status> promise;"); got != len(methods) {
		t.Fatalf("每个 RPC 都须传回 loop 执行状态: got=%d want=%d", got, len(methods))
	}
	for _, name := range []string{"DestroyBattle", "RemoveObserver"} {
		assertGrpcPreservedOnce(t, header, "static void Handle"+name+"(const ::"+name+"Request* request);", "Empty 响应委托声明")
		assertGrpcPreservedOnce(t, cpp, "Handle"+name+"(request);", "Empty 响应委托调用")
		if strings.Contains(cpp, "Handle"+name+"(request, response)") {
			t.Fatalf("Empty 响应不能被传给 Handle%s", name)
		}
	}
}

// 同时覆盖无名旧段和命名新段；改动模板后仍须完整保住准入、许可与停机复核。
func TestGrpcServiceHandler_RegenerationPreservesBattleCustomSections(t *testing.T) {
	setGrpcPreservationTestConfig(t)
	methods := buildBattleGrpcPreservationMethods()
	dir := t.TempDir()
	headerPath := filepath.Join(dir, "battle_node.h")
	cppPath := filepath.Join(dir, "battle_node.cpp")
	header, err := GetGrpcServiceHandlerHeadFromFileStr(headerPath, methods)
	if err != nil {
		t.Fatalf("新建头文件失败: %v", err)
	}
	cpp := GetGrpcServiceHandlerCppStr(cppPath, methods, "", "")

	headerSections := map[string]string{
		"grpc.header.includes": "#include \"battle_admission_gate.h\"",
		"grpc.header.public":   "    bool OpenAdmission();\n    void CloseAdmission();",
		"grpc.header.private":  "    battle_admission::AdmissionGate admission_;",
	}
	for name, body := range headerSections {
		header = fillGrpcPreservationSection(t, header, "", name, body)
	}
	globalCode := "#include \"infra/agones/agones_gameserver_lifecycle.h\"\n" +
		"#include \"logic/battle_room_manager.h\"\n" +
		"namespace {\n" +
		"constexpr char kBattleNotAllocatableMessage[] = \"battle_not_allocatable\";\n" +
		"grpc::Status NotAllocatable() { return grpc::Status(grpc::StatusCode::UNAVAILABLE, kBattleNotAllocatableMessage); }\n" +
		"}\n" +
		"bool BattleNodeImpl::OpenAdmission() { return admission_.Open(); }\n" +
		"void BattleNodeImpl::CloseAdmission() { admission_.Close(); }"
	cpp = fillGrpcPreservationSection(t, cpp, "", "", globalCode)
	customCode := []string{globalCode}
	for _, method := range methods {
		name := method.Method()
		handleCode := "    BattleRoomManager::Instance().Handle" + name + "(*request"
		if !isVoidResponse(method) {
			handleCode += ", *response"
		}
		handleCode += ");"
		cpp = fillGrpcPreservationSection(t, cpp, "void BattleNodeImpl::Handle"+name+"(", "", handleCode)
		customCode = append(customCode, handleCode)
		if name == "CreateBattle" {
			continue
		}
		// 非建房 RPC 也各放不同内容，防止命名段被串到其他方法或被默认空段覆盖。
		for _, phase := range []string{"in_loop", "post_dispatch"} {
			body := "    // 保留 " + name + " 的 " + phase + " 自定义区"
			cpp = fillGrpcPreservationSection(t, cpp, "", "grpc."+name+"."+phase, body)
			customCode = append(customCode, body)
		}
	}
	preDispatch := "    if (admission_.Phase() != battle_admission::AdmissionPhase::kOpen) { return NotAllocatable(); }\n" +
		"    auto& lifecycle = agones::GameServerLifecycle::Instance();\n" +
		"    auto permit = lifecycle.AcquireAllocationPermitBlocking();\n" +
		"    if (!permit) { return NotAllocatable(); }"
	inLoop := "        if (admission_.Phase() != battle_admission::AdmissionPhase::kOpen)\n" +
		"        {\n" +
		"            dispatchStatus = NotAllocatable();\n" +
		"        }"
	postDispatch := "    if (!status.ok()) { LOG_WARN << \"停机期间拒绝建房\"; }"
	cpp = fillGrpcPreservationSection(t, cpp, "grpc::Status BattleNodeImpl::CreateBattle(", "", preDispatch)
	cpp = fillGrpcPreservationSection(t, cpp, "", "grpc.CreateBattle.in_loop", inLoop)
	cpp = fillGrpcPreservationSection(t, cpp, "", "grpc.CreateBattle.post_dispatch", postDispatch)
	customCode = append(customCode, preDispatch, inLoop, postDispatch)
	writeGrpcPreservationFile(t, headerPath, header)
	writeGrpcPreservationFile(t, cppPath, cpp)

	var previousHeader, previousCpp string
	for pass := 0; pass < 3; pass++ {
		generatedHeader, err := GetGrpcServiceHandlerHeadFromFileStr(headerPath, methods)
		if err != nil {
			t.Fatalf("第 %d 次头文件重生成失败: %v", pass+1, err)
		}
		generatedCpp := GetGrpcServiceHandlerCppStr(cppPath, methods, "", "")
		for _, body := range headerSections {
			assertGrpcPreservedOnce(t, generatedHeader, body, "头文件自定义代码")
		}
		for _, body := range customCode {
			assertGrpcPreservedOnce(t, generatedCpp, body, "CPP 自定义代码")
		}
		wrapper := grpcPreservationWrapper(t, generatedCpp, "BattleNode", "CreateBattle")
		assertGrpcPreservationOrder(t, wrapper,
			preDispatch,
			"std::promise<grpc::Status> promise;",
			"loop_.runInLoop([&]",
			"grpc::Status dispatchStatus = grpc::Status::OK;",
			inLoop,
			"if (dispatchStatus.ok())",
			"HandleCreateBattle(request, response);",
			"promise.set_value(dispatchStatus)",
			"auto status = future.get();",
			postDispatch,
			"return status;")
		if pass > 0 && (generatedHeader != previousHeader || generatedCpp != previousCpp) {
			t.Fatalf("第 %d 次重生成发生字节漂移", pass+1)
		}
		previousHeader, previousCpp = generatedHeader, generatedCpp
		writeGrpcPreservationFile(t, headerPath, generatedHeader)
		writeGrpcPreservationFile(t, cppPath, generatedCpp)
	}
}

func TestGrpcServiceHandler_LegacySceneSectionsRemainCompatible(t *testing.T) {
	setGrpcPreservationTestConfig(t)
	methods := buildGrpcHandlerTestMethods()
	legacy := strings.Join([]string{
		"#include \"scene_node_service.h\"",
		_config.Global.Naming.YourCodeBegin,
		"#include \"infra/agones/agones_gameserver_lifecycle.h\"",
		_config.Global.Naming.YourCodeEnd,
		"void SceneNodeGrpcImpl::HandleCreateScene(const ::CreateSceneRequest* request, ::CreateSceneResponse* response)",
		"{",
		_config.Global.Naming.YourCodeBegin,
		"    create_scene_impl(*request, *response);",
		_config.Global.Naming.YourCodeEnd,
		"}",
		"grpc::Status SceneNodeGrpcImpl::CreateScene(grpc::ServerContext* context, const ::CreateSceneRequest* request, ::CreateSceneResponse* response)",
		"{",
		_config.Global.Naming.YourCodeBegin,
		"    auto createPermit = acquire_scene_permit();",
		"    if (!createPermit) { return grpc::Status(grpc::StatusCode::UNAVAILABLE, \"scene_not_allocatable\"); }",
		_config.Global.Naming.YourCodeEnd,
		"    return grpc::Status::OK;",
		"}",
		"void SceneNodeGrpcImpl::HandleDestroyScene(const ::DestroySceneRequest* request)",
		"{",
		_config.Global.Naming.YourCodeBegin,
		"    destroy_scene_impl(*request);",
		_config.Global.Naming.YourCodeEnd,
		"}",
		"",
	}, "\n")
	dst := writeParserTempFile(t, legacy)
	generated := GetGrpcServiceHandlerCppStr(dst, methods, "", "")
	for _, body := range []string{"acquire_scene_permit()", "create_scene_impl(*request, *response);", "destroy_scene_impl(*request);"} {
		assertGrpcPreservedOnce(t, generated, body, "旧 scene 无名守护段")
	}
	create := grpcPreservationWrapper(t, generated, "SceneNodeGrpc", "CreateScene")
	assertGrpcPreservationOrder(t, create, "acquire_scene_permit()", "std::promise<grpc::Status>", "loop_.runInLoop(")
	if strings.Contains(grpcPreservationWrapper(t, generated, "SceneNodeGrpc", "DestroyScene"), "acquire_scene_permit") {
		t.Fatal("没有旧投递前守护段的方法不得捕获其他方法的许可代码")
	}
	writeGrpcPreservationFile(t, dst, generated)
	if next := GetGrpcServiceHandlerCppStr(dst, methods, "", ""); next != generated {
		t.Fatal("旧 scene 文件迁入新骨架后再次生成发生字节漂移")
	}
}

func TestGrpcServiceHandler_HeaderRejectsMalformedNamedSections(t *testing.T) {
	setGrpcPreservationTestConfig(t)
	begin := grpcPreservationBegin("grpc.header.includes")
	end := _config.Global.Naming.YourCodeEnd
	for _, tc := range []struct {
		name string
		code string
	}{
		{"未知头文件段", grpcPreservationBegin("grpc.header.priavte") + "\n    battle_admission::AdmissionGate admission_;\n" + end},
		{"同名重复", begin + "\n#include \"a.h\"\n" + end + "\n" + begin + "\n#include \"b.h\"\n" + end},
		{"未闭合", begin + "\n#include \"a.h\"\n"},
		{"嵌套", begin + "\n" + grpcPreservationBegin("grpc.header.private") + "\n" + end + "\n" + end},
	} {
		t.Run(tc.name, func(t *testing.T) {
			dst := writeParserTempFile(t, tc.code)
			if _, err := GetGrpcServiceHandlerHeadFromFileStr(dst, buildBattleGrpcPreservationMethods()); err == nil {
				t.Fatal("坏守护段必须报错，不能静默丢失或覆盖代码")
			}
		})
	}
}

func TestGrpcServiceHandler_CppRejectsUnconsumedOrMalformedSections(t *testing.T) {
	setGrpcPreservationTestConfig(t)
	begin := _config.Global.Naming.YourCodeBegin
	end := _config.Global.Naming.YourCodeEnd
	namedBegin := grpcPreservationBegin("grpc.CreateBattle.in_loop")
	for _, tc := range []struct {
		name string
		code string
	}{
		{"RPC 名称拼错", grpcPreservationBegin("grpc.CreateBattel.in_loop") + "\n    dispatchStatus = NotAllocatable();\n" + end},
		{"无名外段未闭合", begin + "\n#include \"battle_admission_gate.h\"\n"},
		{"无名段内嵌套命名段", begin + "\n" + namedBegin + "\n    dispatchStatus = NotAllocatable();\n" + end + "\n" + end},
	} {
		t.Run(tc.name, func(t *testing.T) {
			dst := writeParserTempFile(t, tc.code)
			defer func() {
				if recover() == nil {
					t.Fatal("未知或损坏的守护段必须中止生成，不能静默丢弃代码")
				}
			}()
			GetGrpcServiceHandlerCppStr(dst, buildBattleGrpcPreservationMethods(), "", "")
		})
	}
}

func buildBattleGrpcPreservationMethods() RPCMethods {
	fd := &descriptorpb.FileDescriptorProto{Name: proto.String("proto/battle/battle_node.proto")}
	svc := &descriptorpb.ServiceDescriptorProto{Name: proto.String("BattleNode")}
	methods := make(RPCMethods, 0, 5)
	for _, name := range []string{"CreateBattle", "DestroyBattle", "AddObserver", "RemoveObserver", "IssueBattleTicket"} {
		response := "." + name + "Response"
		if name == "DestroyBattle" || name == "RemoveObserver" {
			response = ".Empty"
		}
		methods = append(methods, &MethodInfo{
			ProtoFileInfo: ProtoFileInfo{Fd: fd, ServiceDescriptorProto: svc},
			MethodDescriptorProto: &descriptorpb.MethodDescriptorProto{
				Name: proto.String(name), InputType: proto.String("." + name + "Request"), OutputType: proto.String(response),
			},
		})
	}
	return methods
}

func setGrpcPreservationTestConfig(t *testing.T) {
	t.Helper()
	setParserTestNaming(t)
	oldExt := _config.Global.FileExtensions
	_config.Global.FileExtensions.Proto = ".proto"
	_config.Global.FileExtensions.GrpcHandlerH = ".h"
	_config.Global.FileExtensions.GrpcPbH = ".grpc.pb.h"
	_config.Global.Naming.IncludeBegin = "#include \""
	t.Cleanup(func() { _config.Global.FileExtensions = oldExt })
}

func grpcPreservationBegin(name string) string {
	return _config.Global.Naming.YourCodeBegin + ": " + name
}

func fillGrpcPreservationSection(t *testing.T, code, signature, name, body string) string {
	t.Helper()
	from := 0
	if signature != "" {
		from = strings.Index(code, signature)
		if from < 0 {
			t.Fatalf("夹具缺少函数签名 %q", signature)
		}
	}
	begin := _config.Global.Naming.YourCodeBegin
	if name != "" {
		begin = grpcPreservationBegin(name)
	}
	start := strings.Index(code[from:], begin+"\n")
	if start < 0 {
		t.Fatalf("夹具缺少守护段 %q", begin)
	}
	bodyStart := from + start + len(begin) + 1
	end := strings.Index(code[bodyStart:], _config.Global.Naming.YourCodeEnd)
	if end < 0 {
		t.Fatalf("夹具守护段 %q 没有结束标记", begin)
	}
	bodyEnd := bodyStart + end
	if strings.TrimSpace(code[bodyStart:bodyEnd]) != "" {
		t.Fatalf("夹具守护段 %q 应为空段", begin)
	}
	return code[:bodyStart] + body + "\n" + code[bodyEnd:]
}

func assertGrpcPreservedOnce(t *testing.T, code, want, label string) {
	t.Helper()
	if got := strings.Count(code, want); got != 1 {
		t.Fatalf("%s 应逐字保留一次: got=%d want=%q\n%s", label, got, want, code)
	}
}

func assertGrpcPreservationOrder(t *testing.T, code string, parts ...string) {
	t.Helper()
	from := 0
	for _, part := range parts {
		at := strings.Index(code[from:], part)
		if at < 0 {
			t.Fatalf("执行顺序错误，在前一步之后未找到 %q\n%s", part, code)
		}
		from += at + len(part)
	}
}

func grpcPreservationWrapper(t *testing.T, code, service, method string) string {
	t.Helper()
	start := strings.Index(code, "grpc::Status "+service+"Impl::"+method+"(")
	if start < 0 {
		t.Fatalf("未生成 %s.%s 包装函数", service, method)
	}
	end := strings.Index(code[start+1:], "\ngrpc::Status ")
	if end < 0 {
		return code[start:]
	}
	return code[start : start+1+end]
}

func writeGrpcPreservationFile(t *testing.T, dst, code string) {
	t.Helper()
	if err := os.WriteFile(dst, []byte(code), 0o600); err != nil {
		t.Fatalf("写入临时生成文件失败: %v", err)
	}
}
