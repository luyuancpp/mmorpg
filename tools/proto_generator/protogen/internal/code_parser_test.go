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

func TestReadCodeSections_MethodBlocksNotMisclassifiedAsGlobal(t *testing.T) {
	oldNaming := _config.Global.Naming
	_config.Global.Naming.YourCodeBegin = "///<<< BEGIN WRITING YOUR CODE"
	_config.Global.Naming.YourCodeEnd = "///<<< END WRITING YOUR CODE"
	_config.Global.Naming.YourCodePair = _config.Global.Naming.YourCodeBegin + "\n" + _config.Global.Naming.YourCodeEnd
	_config.Global.Naming.HandlerFile = "Handler"
	t.Cleanup(func() {
		_config.Global.Naming = oldNaming
	})

	methods := buildParserTestMethods("ScenePlayerScene", "LeaveScene", "EnterScene")
	cpp := strings.Join([]string{
		"void ScenePlayerSceneHandler::LeaveScene(int a)",
		"{",
		"///<<< BEGIN WRITING YOUR CODE",
		"\tleave_impl();",
		"///<<< END WRITING YOUR CODE",
		"}",
		"",
		"void ScenePlayerSceneHandler::EnterScene(int a)",
		"{",
		"///<<< BEGIN WRITING YOUR CODE",
		"\tenter_impl();",
		"///<<< END WRITING YOUR CODE",
		"}",
		"",
	}, "\n")

	filePath := writeParserTempFile(t, cpp)
	codeMap, firstCode, err := ReadCodeSectionsFromFile(filePath, &methods, GenerateMethodHandlerNameWrapper, "")
	if err != nil {
		t.Fatalf("ReadCodeSectionsFromFile failed: %v", err)
	}

	if firstCode != _config.Global.Naming.YourCodePair {
		t.Fatalf("unexpected firstCode: got=%q want=%q", firstCode, _config.Global.Naming.YourCodePair)
	}

	leaveKey := "ScenePlayerSceneHandler::LeaveScene("
	enterKey := "ScenePlayerSceneHandler::EnterScene("

	if !strings.Contains(codeMap[leaveKey], "leave_impl") {
		t.Fatalf("leave method code not captured correctly: %q", codeMap[leaveKey])
	}
	if !strings.Contains(codeMap[enterKey], "enter_impl") {
		t.Fatalf("enter method code not captured correctly: %q", codeMap[enterKey])
	}
	if strings.Count(codeMap[leaveKey], _config.Global.Naming.YourCodeEnd) != 1 {
		t.Fatalf("leave method end marker count mismatch: %q", codeMap[leaveKey])
	}
}

func buildParserTestMethods(serviceName string, methodNames ...string) RPCMethods {
	fd := &descriptorpb.FileDescriptorProto{Name: proto.String("proto/scene/player_scene.proto")}
	svc := &descriptorpb.ServiceDescriptorProto{Name: proto.String(serviceName)}

	methods := make(RPCMethods, 0, len(methodNames))
	for _, m := range methodNames {
		methods = append(methods, &MethodInfo{
			ProtoFileInfo: ProtoFileInfo{
				Fd:                     fd,
				ServiceDescriptorProto: svc,
			},
			MethodDescriptorProto: &descriptorpb.MethodDescriptorProto{Name: proto.String(m)},
		})
	}

	return methods
}

func writeParserTempFile(t *testing.T, content string) string {
	t.Helper()
	dir := t.TempDir()
	filePath := filepath.Join(dir, "handler.cpp")
	if err := os.WriteFile(filePath, []byte(content), 0o600); err != nil {
		t.Fatalf("write temp file failed: %v", err)
	}
	return filePath
}
