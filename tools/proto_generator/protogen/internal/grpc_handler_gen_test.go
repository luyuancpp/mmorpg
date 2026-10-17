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

// 回归:commit 6c4021ae5 的重生成吞掉了守护段外的 Agones 建场景许可块。新骨架里准入代码只有一个位置 grpc.<M>.guard。
// 这里钉住三件事:① guard 段在重生成后原样保留,位于 <M>() 里、call->Proceed() 之前,不串到别的方法;
// ② 连续重生成结果完全一致(换行不漂移);③ 新文件为每个方法生成默认空 guard 段。
func TestGrpcServiceHandlerCpp_PreservesGuardSection(t *testing.T) {
	setGrpcPreservationTestConfig(t)
	methods := buildGrpcHandlerTestMethods()
	dst := filepath.Join(t.TempDir(), "scene_node_service.cpp")

	fresh := GetGrpcServiceHandlerCppStr(dst, methods, "", "")
	if got, want := strings.Count(fresh, _config.Global.Naming.YourCodeBegin), 1+2*len(methods); got != want {
		t.Fatalf("fresh file section count mismatch: got=%d want=%d\n%s", got, want, fresh)
	}

	guard := "    if (call->ServerDraining())\n" +
		"    {\n" +
		"        call->Reject(muduo_grpc::Status(muduo_grpc::StatusCode::kUnavailable, kSceneNodeShuttingDownMessage));\n" +
		"        return;\n" +
		"    }"
	writeGrpcPreservationFile(t, dst, fillGrpcPreservationSection(t, fresh, "", "grpc.CreateScene.guard", guard))
	first := GetGrpcServiceHandlerCppStr(dst, methods, "", "")
	assertGrpcPreservedOnce(t, first, guard, "guard 段")
	assertGrpcPreservationOrder(t, grpcGuardFunction(t, first, "SceneNodeGrpc", "CreateScene"),
		"[[maybe_unused]] const ::CreateSceneRequest* request = &call->request();",
		grpcPreservationBegin("grpc.CreateScene.guard"),
		guard,
		_config.Global.Naming.YourCodeEnd,
		"call->Proceed();")
	for _, method := range methods[1:] {
		if strings.Contains(grpcGuardFunction(t, first, "SceneNodeGrpc", method.Method()), "kSceneNodeShuttingDownMessage") {
			t.Fatalf("CreateScene 的 guard 段串到了 %s", method.Method())
		}
	}

	if err := os.WriteFile(dst, []byte(first), 0o600); err != nil {
		t.Fatalf("rewrite dst failed: %v", err)
	}
	second := GetGrpcServiceHandlerCppStr(dst, methods, "", "")
	if second != first {
		t.Fatalf("regeneration is not idempotent\n--- first ---\n%s\n--- second ---\n%s", first, second)
	}
}

// 有 package 的服务(scene)路由前缀带 package,没有 package 的(battle)不带;与客户端 k<Svc><Method>GrpcPath 同一规则。
func TestGrpcServiceHandler_MethodPathsAndMessageTypes(t *testing.T) {
	setGrpcPreservationTestConfig(t)
	sceneMethods := buildGrpcHandlerTestMethods()
	for _, method := range sceneMethods {
		if got, want := grpcServerMethodPath(method), "/scene_node.SceneNodeGrpc/"+method.Method(); got != want {
			t.Fatalf("scene :path got=%q want=%q", got, want)
		}
	}
	for _, method := range buildBattleGrpcPreservationMethods() {
		if got, want := grpcServerMethodPath(method), "/BattleNode/"+method.Method(); got != want {
			t.Fatalf("battle :path got=%q want=%q", got, want)
		}
	}

	dir := t.TempDir()
	header, err := GetGrpcServiceHandlerHeadFromFileStr(filepath.Join(dir, "scene_node_service.h"), sceneMethods)
	if err != nil {
		t.Fatalf("生成 scene 头文件失败: %v", err)
	}
	cpp := GetGrpcServiceHandlerCppStr(filepath.Join(dir, "scene_node_service.cpp"), sceneMethods, "", "")
	for _, want := range []string{
		"#include \"proto/scene_manager/scene_node_service.pb.h\"",
		"class SceneNodeGrpcImpl final : public muduo_grpc::Service",
		"static constexpr std::string_view kReleasePlayerPath = \"/scene_node.SceneNodeGrpc/ReleasePlayer\";",
		"using ReleasePlayerCall = GrpcUnaryCall<::scene_node::ReleasePlayerRequest, ::Empty>;",
		"using AssetAckReceiptCall = GrpcUnaryCall<::AssetOpRequest, ::AssetOpResponse>;",
		"static void HandleReleasePlayer(const ::scene_node::ReleasePlayerRequest* request);",
	} {
		assertGrpcPreservedOnce(t, header, want, "scene 头文件")
	}
	assertGrpcPreservedOnce(t, cpp,
		"[](const ::scene_node::ReleasePlayerRequest* request, ::Empty* /*response*/) { HandleReleasePlayer(request); });",
		"scene Empty 响应委托")
	assertNoOfficialGrpcServerTokens(t, header, "scene 头文件")
	assertNoOfficialGrpcServerTokens(t, cpp, "scene cpp")
	for _, method := range sceneMethods {
		assertGrpcPreservedOnce(t, cpp, GenerateGrpcHandlerNameWrapper(method, ""), "Handle* 查找键只能出现在定义行")
	}
}

// 服务端只支持 unary:有流式方法时头文件返回 error、cpp panic,两份都不产出。
func TestGrpcServiceHandler_StreamingMethodFailsClosed(t *testing.T) {
	for _, tc := range []struct {
		name string
		mark func(*descriptorpb.MethodDescriptorProto)
	}{
		{"server streaming", func(m *descriptorpb.MethodDescriptorProto) { m.ServerStreaming = proto.Bool(true) }},
		{"client streaming", func(m *descriptorpb.MethodDescriptorProto) { m.ClientStreaming = proto.Bool(true) }},
	} {
		t.Run(tc.name, func(t *testing.T) {
			setGrpcPreservationTestConfig(t)
			methods := buildBattleGrpcPreservationMethods()
			tc.mark(methods[2].MethodDescriptorProto)
			dir := t.TempDir()
			if _, err := GetGrpcServiceHandlerHeadFromFileStr(filepath.Join(dir, "battle_node.h"), methods); err == nil ||
				!strings.Contains(err.Error(), "BattleNode.DestroyBattle") {
				t.Fatalf("流式方法必须让头文件生成失败并点名方法: %v", err)
			}
			if _, err := GetGrpcServiceHandlerHeadStr(methods); err == nil {
				t.Fatal("无文件版本的头文件生成同样必须拒绝流式方法")
			}
			message := recoverGrpcGenerationPanic(t, func() {
				GetGrpcServiceHandlerCppStr(filepath.Join(dir, "battle_node.cpp"), methods, "", "")
			})
			if !strings.Contains(message, "BattleNode.DestroyBattle") {
				t.Fatalf("cpp 拒绝信息应点名流式方法: %s", message)
			}
		})
	}
}

// 「空」= 去掉标记行后不剩非空白字符;注释也算非空,标记行上夹带的代码也算非空。
func TestIsEmptyGrpcCodeSection(t *testing.T) {
	setParserTestNaming(t)
	begin := _config.Global.Naming.YourCodeBegin
	end := _config.Global.Naming.YourCodeEnd
	for _, tc := range []struct {
		name  string
		code  string
		empty bool
	}{
		{"默认空对", _config.Global.Naming.YourCodePair, true},
		{"缩进的命名标记与空白行", "        " + begin + ": grpc.CreateBattle.in_loop\n    \n\t\r\n        " + end, true},
		{"空字符串", "", true},
		{"注释算非空", begin + "\n    // 迁移前的说明\n" + end, false},
		{"代码", begin + "\n    dispatchStatus = NotAllocatable();\n" + end, false},
		{"标记行夹带代码", begin + "\n    x(); " + end, false},
	} {
		t.Run(tc.name, func(t *testing.T) {
			if got := isEmptyGrpcCodeSection(tc.code); got != tc.empty {
				t.Fatalf("isEmptyGrpcCodeSection(%q)=%v want=%v", tc.code, got, tc.empty)
			}
		})
	}
}

// buildGrpcHandlerTestMethods 照 proto/scene_manager/scene_node_service.proto 的 SceneNodeGrpc(package scene_node,9 个 unary)。
func buildGrpcHandlerTestMethods() RPCMethods {
	fd := &descriptorpb.FileDescriptorProto{
		Name:    proto.String("proto/scene_manager/scene_node_service.proto"),
		Package: proto.String("scene_node"),
	}
	svc := &descriptorpb.ServiceDescriptorProto{Name: proto.String("SceneNodeGrpc")}
	newMethod := func(name, in, out string) *MethodInfo {
		return &MethodInfo{
			ProtoFileInfo: ProtoFileInfo{Fd: fd, ServiceDescriptorProto: svc},
			MethodDescriptorProto: &descriptorpb.MethodDescriptorProto{
				Name:       proto.String(name),
				InputType:  proto.String(in),
				OutputType: proto.String(out),
			},
		}
	}
	return RPCMethods{
		newMethod("CreateScene", ".CreateSceneRequest", ".CreateSceneResponse"),
		newMethod("DestroyScene", ".DestroySceneRequest", ".Empty"),
		newMethod("ReleasePlayer", ".scene_node.ReleasePlayerRequest", ".Empty"),
		newMethod("PrepareBattle", ".PrepareBattleRequest", ".PrepareBattleResponse"),
		newMethod("CancelBattlePrepare", ".CancelBattlePrepareRequest", ".Empty"),
		newMethod("AssetDebit", ".AssetOpRequest", ".AssetOpResponse"),
		newMethod("AssetAbortDebit", ".AssetOpRequest", ".AssetOpResponse"),
		newMethod("AssetCredit", ".AssetOpRequest", ".AssetOpResponse"),
		newMethod("AssetAckReceipt", ".AssetOpRequest", ".AssetOpResponse"),
	}
}
