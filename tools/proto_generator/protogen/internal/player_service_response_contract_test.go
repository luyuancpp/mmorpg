package internal

import (
	"strings"
	"testing"

	_config "protogen/internal/config"

	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"
)

func playerResponseContractMethods(t *testing.T, responses ...string) RPCMethods {
	t.Helper()
	original := _config.Global
	t.Cleanup(func() { _config.Global = original })
	_config.Global = _config.Config{}
	_config.Global.FileExtensions.Proto = ".proto"
	_config.Global.FileExtensions.PbH = ".pb.h"
	_config.Global.Naming.IncludeBegin = "#include \""
	_config.Global.Naming.PlayerServiceIncludeName = "#include \"rpc/player_service_interface.h\""
	_config.Global.Naming.MacroReturnIncludeName = "#include \"macros/return_define.h\""
	_config.Global.Naming.PlayerMethodController = "entt::entity player,"
	_config.Global.Naming.EmptyResponse = "Empty"
	file := &descriptorpb.FileDescriptorProto{Name: proto.String("proto/scene/player_contract.proto")}
	service := &descriptorpb.ServiceDescriptorProto{Name: proto.String("PlayerContract")}
	methods := make(RPCMethods, 0, len(responses))
	for _, response := range responses {
		methods = append(methods, &MethodInfo{
			ProtoFileInfo: ProtoFileInfo{Fd: file, ServiceDescriptorProto: service},
			MethodDescriptorProto: &descriptorpb.MethodDescriptorProto{
				Name:       proto.String("Call" + strings.ReplaceAll(response, ".", "")),
				InputType:  proto.String(".Request"),
				OutputType: proto.String(response),
			},
		})
	}
	return methods
}

func TestPlayerResponseContract_AllNonEmptyResponsesHaveCompileTimeGuard(t *testing.T) {
	methods := playerResponseContractMethods(t, ".Reply", ".Empty", ".EmptyResult", ".domain.Empty", ".domain.Outer.Reply")
	header, err := GetPlayerServiceHeadStr(methods)
	if err != nil {
		t.Fatal(err)
	}
	if got := strings.Count(header, "static_assert("); got != 4 {
		t.Fatalf("非 Empty 的四个应答必须分别受到编译期守卫保护,实际=%d\n%s", got, header)
	}
	for _, response := range []string{"::Reply", "::EmptyResult", "::domain::Empty", "::domain::Outer::Reply"} {
		// 缺少字段时 decltype 无法编译;字段类型不对时 is_same_v 必须为 false。
		guard := "static_assert(std::is_same_v<decltype(std::declval<" + response + "&>().mutable_error_message()), ::TipInfoMessage*>"
		if !strings.Contains(header, guard) {
			t.Fatalf("应答 %s 缺少字段存在性及 TipInfoMessage 类型约束\n%s", response, header)
		}
	}
	if strings.Contains(header, "std::declval<::Empty&>") {
		t.Fatalf("真正的 Empty 应答不能被要求提供 error_message\n%s", header)
	}
	for _, include := range []string{"#include <type_traits>", "#include <utility>"} {
		if !strings.Contains(header, include) {
			t.Fatalf("编译期守卫必须显式包含 %s\n%s", include, header)
		}
	}
}

func TestPlayerResponseContract_ProtobufEmptyDoesNotRequireErrorMessage(t *testing.T) {
	// 真实玩家生命周期 RPC 使用标准 Empty；近似名字仍是业务应答，不能按后缀放行。
	methods := playerResponseContractMethods(t, ".google.protobuf.Empty", ".google.protobuf.EmptyResult", ".domain.Empty")
	header, err := GetPlayerServiceHeadStr(methods)
	if err != nil {
		t.Fatal(err)
	}
	if got := strings.Count(header, "static_assert("); got != 2 {
		t.Fatalf("标准 Empty 不要求错误字段，其余两个业务应答必须保留编译期守卫,实际=%d\n%s", got, header)
	}
	if strings.Contains(header, "std::declval<::google::protobuf::Empty&>") {
		t.Fatalf("标准 Empty 应答不能被要求提供 error_message\n%s", header)
	}
	for _, response := range []string{"::google::protobuf::EmptyResult", "::domain::Empty"} {
		guard := "static_assert(std::is_same_v<decltype(std::declval<" + response + "&>().mutable_error_message()), ::TipInfoMessage*>"
		if !strings.Contains(header, guard) {
			t.Fatalf("业务应答 %s 必须保留 TipInfoMessage 类型约束\n%s", response, header)
		}
	}
	if call := methods[0].Method() + "(player,"; strings.Count(header, call) != 1 {
		t.Fatalf("标准 Empty 方法必须恰好派发一次\n%s", header)
	}
}

func TestPlayerResponseContract_DispatchDoesNotTransferOrCreateErrors(t *testing.T) {
	methods := playerResponseContractMethods(t, ".Reply", ".Empty")
	header, err := GetPlayerServiceHeadStr(methods)
	if err != nil {
		t.Fatal(err)
	}
	for _, forbidden := range []string{"TRANSFER_ERROR_MESSAGE", "globalRegistry", "GlobalEntity", "CopyFrom(", "->mutable_error_message()", "set_id("} {
		if strings.Contains(header, forbidden) {
			t.Fatalf("派发层不得搬运或创建请求错误:%s\n%s", forbidden, header)
		}
	}
	for _, method := range methods {
		call := method.Method() + "(player,"
		if strings.Count(header, call) != 1 {
			t.Fatalf("每个方法必须恰好派发一次:%s\n%s", method.Method(), header)
		}
	}
}
