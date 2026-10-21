package unity

import (
	"bytes"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"protogen/internal"
	"protogen/logger"

	messageoption "github.com/luyuancpp/protooption"
	"go.uber.org/zap"
	"google.golang.org/protobuf/encoding/protowire"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"
)

// 内存描述符沿正式分类与生成入口流转，不调用 protoc，也不触碰客户端仓。
func rpcRegressionFixture() (internal.RPCServiceInfoList, []*descriptorpb.FileDescriptorProto) {
	service := &descriptorpb.ServiceDescriptorProto{
		Name: proto.String("GuildService"), Options: &descriptorpb.ServiceOptions{},
	}
	proto.SetExtension(service.Options, messageoption.E_OptionIsClientProtocolService, true)
	file := &descriptorpb.FileDescriptorProto{
		Name: proto.String("proto/test/client.proto"), Package: proto.String("fixturepb"),
		Options: &descriptorpb.FileOptions{CsharpNamespace: proto.String("Example.Api")},
		MessageType: []*descriptorpb.DescriptorProto{
			{Name: proto.String("Request")}, {Name: proto.String("Response")}, {Name: proto.String("Changed")},
		},
		Service: []*descriptorpb.ServiceDescriptorProto{service},
		SourceCodeInfo: &descriptorpb.SourceCodeInfo{Location: []*descriptorpb.SourceCodeInfo_Location{{
			Path: []int32{6, 0, 2, 0}, LeadingComments: proto.String(" 读取 <name> & 角色。\n"),
		}}},
	}
	info := &internal.RPCServiceInfo{ProtoFileInfo: internal.ProtoFileInfo{Fd: file, ServiceDescriptorProto: service}}
	for index, spec := range []struct {
		name, output string
		kind         internal.ClientRpcKind
	}{
		{"Read", ".fixturepb.Response", internal.ClientRpcRequest},
		{"Move", ".Empty", internal.ClientRpcOneWay},
		{"NotifyChanged", ".Empty", internal.ClientRpcPush},
		{"Disconnect", ".Empty", internal.ClientRpcServerOnly},
		{"CancelQueue", ".Empty", internal.ClientRpcRequest},
	} {
		input := ".fixturepb.Request"
		if spec.kind == internal.ClientRpcPush {
			input = ".fixturepb.Changed"
		}
		method := &descriptorpb.MethodDescriptorProto{
			Name: proto.String(spec.name), InputType: proto.String(input), OutputType: proto.String(spec.output),
			Options: &descriptorpb.MethodOptions{},
		}
		method.Options.ProtoReflect().SetUnknown(protowire.AppendVarint(
			protowire.AppendTag(nil, internal.OptionClientRpcKindNumber, protowire.VarintType), uint64(spec.kind)))
		service.Method = append(service.Method, method)
		info.Methods = append(info.Methods, &internal.MethodInfo{
			ProtoFileInfo: info.ProtoFileInfo, MethodDescriptorProto: method, Id: uint64(index),
		})
	}
	empty := &descriptorpb.FileDescriptorProto{
		Name:        proto.String("proto/common/base/empty.proto"),
		MessageType: []*descriptorpb.DescriptorProto{{Name: proto.String("Empty")}},
	}
	return internal.RPCServiceInfoList{info}, []*descriptorpb.FileDescriptorProto{file, empty}
}

func rpcRegressionGenerate(t *testing.T, root string, services internal.RPCServiceInfoList,
	files []*descriptorpb.FileDescriptorProto) (rpcDirLayout, int, error) {
	t.Helper()
	classified, err := internal.ClassifyClientRpcServices(services, func(method *internal.MethodInfo) bool {
		return method.Method() == "Move"
	})
	if err != nil {
		return rpcDirLayout{}, 0, err
	}
	layout, err := newRpcDirLayout(root, filepath.Join(root, "Assets", "Scripts", "Net", "Generated", "Rpc"))
	if err != nil {
		return rpcDirLayout{}, 0, err
	}
	count, err := generateUnityClientRpc(classified, files, layout,
		filepath.Join("..", "..", "template", "unity_client_rpc.cs.tmpl"))
	return layout, count, err
}

func rpcRegressionRead(t *testing.T, name string) []byte {
	t.Helper()
	content, err := os.ReadFile(name)
	if err != nil {
		t.Fatal(err)
	}
	return content
}

func rpcRegressionLogger(t *testing.T) {
	t.Helper()
	previous := logger.Global
	logger.Global = zap.NewNop()
	t.Cleanup(func() { logger.Global = previous })
}

func TestUnityClientRpcGeneratesRequestOneWayPushAndOmitsServerOnly(t *testing.T) {
	rpcRegressionLogger(t)
	services, files := rpcRegressionFixture()
	layout, count, err := rpcRegressionGenerate(t, t.TempDir(), services, files)
	if err != nil || count != 1 {
		t.Fatalf("生成结果 count=%d err=%v，预期仅一个服务文件", count, err)
	}
	content := string(rpcRegressionRead(t, filepath.Join(layout.dir, "GuildServiceRpc.cs")))
	for _, expected := range []string{
		"public const uint ReadMessageId = 0;",
		"public static void Read(IRpcChannel channel, global::Example.Api.Request request,",
		"RpcDone<global::Example.Api.Response> done)",
		"channel.CallMethod(ReadMessageId, request, global::Example.Api.Response.Parser, done)",
		"public static void Move(IRpcChannel channel, global::Example.Api.Request request)",
		"channel.SendOneWay(MoveMessageId, request)",
		"public static void OnNotifyChanged(IRpcChannel channel, Action<global::Example.Api.Changed> handler,",
		"channel.RegisterNotify(NotifyChangedMessageId, mc =>",
		"global::Example.Api.Changed.Parser.ParseFrom(mc.SerializedMessage)",
		"RpcDone<global::Empty> done)",
		"channel.CallMethod(CancelQueueMessageId, request, global::Empty.Parser, done)",
		"/// 读取 &lt;name&gt; &amp; 角色。",
	} {
		if !strings.Contains(content, expected) {
			t.Errorf("缺少契约输出 %q", expected)
		}
	}
	if strings.Contains(content, "Disconnect") {
		t.Error("SERVER_ONLY 不得生成方法或消息号常量")
	}
	if strings.Contains(content, "this IRpcChannel") {
		t.Error("调用桩不得生成为扩展方法")
	}
}

func TestUnityClientRpcRemovesStaleStubsAndKeepsStableMeta(t *testing.T) {
	rpcRegressionLogger(t)
	root := t.TempDir()
	rpcDir := filepath.Join(root, "Assets", "Scripts", "Net", "Generated", "Rpc")
	if err := os.MkdirAll(rpcDir, 0o755); err != nil {
		t.Fatal(err)
	}
	stale := filepath.Join(rpcDir, "RemovedServiceRpc.cs")
	staleSource := "// <auto-generated>\n//   Generated by protogen from proto/removed.proto. DO NOT EDIT.\n// </auto-generated>\n"
	for name, content := range map[string]string{
		stale: staleSource, stale + ".meta": "fileFormatVersion: 2\nguid: 00000000000000000000000000000000\n",
		filepath.Join(rpcDir, "Handwritten.cs"): "// 手写文件不归生成器清理\n",
	} {
		if err := os.WriteFile(name, []byte(content), 0o644); err != nil {
			t.Fatal(err)
		}
	}
	services, files := rpcRegressionFixture()
	layout, _, err := rpcRegressionGenerate(t, root, services, files)
	if err != nil {
		t.Fatal(err)
	}
	for _, name := range []string{stale, stale + ".meta"} {
		if _, err := os.Stat(name); !os.IsNotExist(err) {
			t.Errorf("已退役生成桩必须删除: %s, err=%v", name, err)
		}
	}
	if got := string(rpcRegressionRead(t, filepath.Join(rpcDir, "Handwritten.cs"))); got != "// 手写文件不归生成器清理\n" {
		t.Fatal("清理旧桩改写了手写文件")
	}
	paths := []string{filepath.Join(layout.dir, "GuildServiceRpc.cs"), filepath.Join(layout.dir, "GuildServiceRpc.cs.meta"), layout.dir + ".meta"}
	before := make([][]byte, len(paths))
	for index, name := range paths {
		before[index] = rpcRegressionRead(t, name)
	}
	// 这两个固定值来自设计规定的路径盐值，不从被测 GUID 函数计算期望值。
	if !bytes.Contains(before[1], []byte("guid: f8c3a4472e314b57c73e4a297adf4aa7\n")) {
		t.Error("脚本 meta GUID 不符合 protogen: + 相对 Assets 路径的既定值")
	}
	if !bytes.Contains(before[2], []byte("guid: bba74473eac1dc59d05e9d357073d8da\n")) {
		t.Error("目录 meta GUID 不符合既定值")
	}
	if _, _, err := rpcRegressionGenerate(t, root, services, files); err != nil {
		t.Fatal(err)
	}
	for index, name := range paths {
		if !bytes.Equal(before[index], rpcRegressionRead(t, name)) {
			t.Errorf("同一输入再次生成内容不稳定: %s", name)
		}
	}
}

func TestUnityClientRpcSelectsServiceOptionRatherThanName(t *testing.T) {
	rpcRegressionLogger(t)
	services, files := rpcRegressionFixture()
	unmarked := &descriptorpb.ServiceDescriptorProto{Name: proto.String("ClientPlayerUnmarked")}
	method := &descriptorpb.MethodDescriptorProto{
		Name: proto.String("Read"), InputType: proto.String(".fixturepb.Request"), OutputType: proto.String(".fixturepb.Response"),
	}
	unmarked.Method = []*descriptorpb.MethodDescriptorProto{method}
	files[0].Service = append(files[0].Service, unmarked)
	info := internal.ProtoFileInfo{Fd: files[0], ServiceDescriptorProto: unmarked}
	services = append(services, &internal.RPCServiceInfo{
		ProtoFileInfo: info, Methods: internal.RPCMethods{{ProtoFileInfo: info, MethodDescriptorProto: method, Id: 19}},
	})
	layout, count, err := rpcRegressionGenerate(t, t.TempDir(), services, files)
	if err != nil || count != 1 {
		t.Fatalf("服务必须按显式 option 筛选: count=%d err=%v", count, err)
	}
	if _, err := os.Stat(filepath.Join(layout.dir, "GuildServiceRpc.cs")); err != nil {
		t.Error("不含 ClientPlayer 的已标注服务未生成")
	}
	if _, err := os.Stat(filepath.Join(layout.dir, "ClientPlayerUnmarkedRpc.cs")); !os.IsNotExist(err) {
		t.Errorf("未标注服务不应生成: err=%v", err)
	}
}

func TestUnityClientRpcMapsNestedAndNamespacedTypes(t *testing.T) {
	files := []*descriptorpb.FileDescriptorProto{{
		Name: proto.String("proto/test/types.proto"), Package: proto.String("sample_proto.v2"),
		MessageType: []*descriptorpb.DescriptorProto{{
			Name: proto.String("Outer"), NestedType: []*descriptorpb.DescriptorProto{{Name: proto.String("Inner")}},
		}},
	}, {
		Name: proto.String("proto/test/override.proto"), Package: proto.String("ignored"),
		Options:     &descriptorpb.FileOptions{CsharpNamespace: proto.String("Custom.Types")},
		MessageType: []*descriptorpb.DescriptorProto{{Name: proto.String("Reply")}},
	}}
	index := newCSharpTypeIndex(files)
	for input, expected := range map[string]string{
		".sample_proto.v2.Outer.Inner": "global::SampleProto.V2.Outer.Types.Inner",
		".ignored.Reply":               "global::Custom.Types.Reply",
	} {
		actual, err := index.resolve(input)
		if err != nil || actual != expected {
			t.Errorf("类型映射 %s: got=%q err=%v want=%q", input, actual, err, expected)
		}
	}
	if _, err := index.resolve(".not_present.Request"); err == nil {
		t.Error("缺失的描述符类型必须拒绝，不能输出猜测的 C# 类型")
	}
}
