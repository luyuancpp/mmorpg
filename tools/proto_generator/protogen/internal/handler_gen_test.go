package internal

import (
	"path/filepath"
	"strings"
	"testing"

	_config "protogen/internal/config"

	"google.golang.org/protobuf/proto"
)

func TestServiceHandlerCppFreshMethodKeepsClosingBraceOutsideGuardComment(t *testing.T) {
	setParserTestNaming(t)
	previousExtensions := _config.Global.FileExtensions
	t.Cleanup(func() { _config.Global.FileExtensions = previousExtensions })
	_config.Global.FileExtensions.Proto = ".proto"
	_config.Global.FileExtensions.HandlerH = "_handler.h"
	_config.Global.Naming.GoogleMethodController = "::google::protobuf::RpcController* controller, "
	methods := buildParserTestMethods("SceneScene", "GmDescribeEntity")
	methods[0].MethodDescriptorProto.InputType = proto.String(".GmDescribeEntityRequest")
	methods[0].MethodDescriptorProto.OutputType = proto.String(".GmDescribeEntityResponse")
	fresh := GetServiceHandlerCppStr(filepath.Join(t.TempDir(), "not_created.cpp"), methods, "", "")
	if strings.Contains(fresh, _config.Global.Naming.YourCodeEnd+"}") ||
		!strings.Contains(fresh, _config.Global.Naming.YourCodeEnd+"\n}") {
		t.Fatalf("新 RPC 的结束守护标记与函数右括号必须分行:\n%s", fresh)
	}
}
