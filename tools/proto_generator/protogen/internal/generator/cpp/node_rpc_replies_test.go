package cpp

import (
	"os"
	"path/filepath"
	"strings"
	"sync"
	"testing"

	"protogen/internal"
	_config "protogen/internal/config"
	"protogen/logger"

	messageoption "github.com/luyuancpp/protooption"
	"go.uber.org/zap"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"
)

// 节点间 RPC0 的回包改在调用点的每次调用 lambda 里完成(docs/design/muduo-rpc-channel.md §9.2):
// 生成器不得再为 Gate / Scene / SceneScene 产出 rpc_replies 回包文件与 InitReply 注册文件。
// 生成器从不删除过期文件,一旦恢复生成,下一次运行就会把已删除的回包空壳与全局回包表注册写回来。
// 玩家回包链(rpc_replies/player)本批保留,gate 的 rpc_replies/register_handler.cpp 照旧生成。

func setupNodeRepliesGeneration(t *testing.T) _config.MethodHandlerDirs {
	t.Helper()
	originalConfig, originalLogger, originalServices := _config.Global, logger.Global, internal.GlobalRPCServiceList
	t.Cleanup(func() {
		_config.Global, logger.Global, internal.GlobalRPCServiceList = originalConfig, originalLogger, originalServices
	})
	_config.Global = _config.Config{}
	logger.Global = zap.NewNop()
	internal.GlobalRPCServiceList = nil

	root := t.TempDir()
	dir := func(parts ...string) string {
		return filepath.ToSlash(filepath.Join(append([]string{root}, parts...)...)) + "/"
	}
	// 每个输出目录都指到临时根下:目录为空串时生成器会写进当前工作目录。
	dirs := _config.MethodHandlerDirs{
		Robot:                  dir("robot"),
		SceneNode:              dir("scene", "handler", "rpc"),
		SceneNodePlayer:        dir("scene", "handler", "rpc", "player"),
		SceneNodeReplied:       dir("scene", "rpc_replies"),
		SceneNodePlayerReplied: dir("scene", "rpc_replies", "player"),
		SceneNodeGrpc:          dir("scene", "handler", "grpc"),
		BattleNodeGrpc:         dir("battle", "handler", "grpc"),
		GateNode:               dir("gate", "handler", "rpc"),
		GateNodeReplied:        dir("gate", "rpc_replies"),
		GateNodePlayer:         dir("gate", "handler", "rpc", "player"),
		GateNodePlayerReplied:  dir("gate", "rpc_replies", "player"),
	}
	_config.Global.PathLists.MethodHandlerDirectories = dirs

	// 取值与 etc/proto_gen.yaml 一致。
	extensions := &_config.Global.FileExtensions
	extensions.Proto = ".proto"
	extensions.PbH = ".pb.h"
	extensions.HandlerH = "_handler.h"
	extensions.HandlerCpp = "_handler.cpp"
	extensions.RepliedHandlerHeaderExtension = "_response_handler.h"
	extensions.CppRepliedHandlerEx = "_response_handler.cpp"
	extensions.RegisterHandlerCppExtension = "register_handler.cpp"
	extensions.RegisterRepliedHandlerCppExtension = "register_response_handler.cpp"

	naming := &_config.Global.Naming
	naming.MessageId = "MessageId"
	naming.HandlerFile = "Handler"
	naming.RepliedHandlerFile = "Reply"
	naming.EmptyResponse = "Empty"
	naming.IncludeBegin = "#include \""
	naming.IncludeEndLine = "\"\n"
	naming.GoogleMethodController = "::google::protobuf::RpcController* controller, "
	naming.PlayerMethodController = "entt::entity player,"
	naming.PlayerServiceIncludeName = "#include \"rpc/player_service_interface.h\"\n"
	naming.PlayerServiceRepliedIncludeName = "#include \"rpc/player_rpc_response_handler.h\"\n"
	naming.YourCodeBegin = "///<<< BEGIN WRITING YOUR CODE"
	naming.YourCodeEnd = "///<<< END WRITING YOUR CODE"
	naming.YourCodePair = naming.YourCodeBegin + "\n" + naming.YourCodeEnd
	return dirs
}

func nodeServiceMethods(path string, node messageoption.NodeType, serviceName string, isPlayer bool, methodName string) internal.RPCMethods {
	method := buildMethod(path, node, true, isPlayer, false)
	method.ServiceDescriptorProto.Name = proto.String(serviceName)
	method.MethodDescriptorProto = &descriptorpb.MethodDescriptorProto{
		Name:       proto.String(methodName),
		InputType:  proto.String("." + methodName + "Request"),
		OutputType: proto.String("." + methodName + "Response"),
	}
	return asMethods(method)
}

// generatedRegularFiles 列出目录下的普通文件名(不含子目录,按名字排序);目录不存在视为空。
func generatedRegularFiles(t *testing.T, dir string) []string {
	t.Helper()
	entries, err := os.ReadDir(dir)
	if os.IsNotExist(err) {
		return nil
	}
	if err != nil {
		t.Fatal(err)
	}
	var names []string
	for _, entry := range entries {
		if !entry.IsDir() {
			names = append(names, entry.Name())
		}
	}
	return names
}

func assertGeneratedFiles(t *testing.T, dir string, want ...string) {
	t.Helper()
	got := generatedRegularFiles(t, dir)
	if strings.Join(got, ",") != strings.Join(want, ",") {
		t.Fatalf("files in %s: got %v, want %v", dir, got, want)
	}
}

func readGeneratedFile(t *testing.T, path string) string {
	t.Helper()
	data, err := os.ReadFile(path)
	if err != nil {
		t.Fatal(err)
	}
	return string(data)
}

func TestNodeLevelServicesGenerateNoMuduoReplyHandlers(t *testing.T) {
	dirs := setupNodeRepliesGeneration(t)
	for _, methods := range []internal.RPCMethods{
		nodeServiceMethods("proto/gate/gate_service.proto", messageoption.NodeType_NODE_GATE, "Gate", false, "NodeHandshake"),
		nodeServiceMethods("proto/scene/scene.proto", messageoption.NodeType_NODE_SCENE, "Scene", false, "NodeHandshake"),
		nodeServiceMethods("proto/scene/scene_admin.proto", messageoption.NodeType_NODE_SCENE, "SceneScene", false, "GmGracefulShutdown"),
	} {
		var wg sync.WaitGroup
		ProcessAllHandlers(&wg, methods)
		wg.Wait()
	}

	// handler/rpc 保留:三个服务的 Service 实现照常生成,头文件带 done 用法的类级注释。
	assertGeneratedFiles(t, dirs.GateNode, "gate_service_handler.cpp", "gate_service_handler.h")
	assertGeneratedFiles(t, dirs.SceneNode, "scene_admin_handler.cpp", "scene_admin_handler.h", "scene_handler.cpp", "scene_handler.h")
	header := readGeneratedFile(t, dirs.SceneNode+"scene_handler.h")
	if !strings.Contains(header, "RpcDoneCallback::Adopt(done)") || !strings.Contains(header, "class SceneHandler : public ::Scene") {
		t.Fatalf("node handler header lost the done contract comment or the class:\n%s", header)
	}
	if cpp := readGeneratedFile(t, dirs.SceneNode+"scene_handler.cpp"); strings.Contains(cpp, "RpcDoneCallback") {
		t.Fatalf("handler cpp must not carry the comment (it would disturb guard-section parsing):\n%s", cpp)
	}

	// rpc_replies 不再出现节点级回包:gate 的 scene_response_handler / scene_admin_response_handler、
	// scene 的 gate_service_response_handler。
	assertGeneratedFiles(t, dirs.GateNodeReplied)
	assertGeneratedFiles(t, dirs.SceneNodeReplied)
}

func TestPlayerReplyChainStillGenerated(t *testing.T) {
	dirs := setupNodeRepliesGeneration(t)
	var wg sync.WaitGroup
	ProcessAllHandlers(&wg, nodeServiceMethods("proto/scene/player_lifecycle.proto", messageoption.NodeType_NODE_SCENE, "ScenePlayer", true, "ExitGame"))
	wg.Wait()

	assertGeneratedFiles(t, dirs.SceneNodePlayerReplied, "player_lifecycle_response_handler.cpp", "player_lifecycle_response_handler.h")
	header := readGeneratedFile(t, dirs.SceneNodePlayerReplied+"player_lifecycle_response_handler.h")
	if !strings.Contains(header, "class ScenePlayerReply : public ::PlayerServiceReplied") {
		t.Fatalf("player reply header changed:\n%s", header)
	}
	cpp := readGeneratedFile(t, dirs.SceneNodePlayerReplied+"player_lifecycle_response_handler.cpp")
	if !strings.Contains(cpp, "ScenePlayerReply::ExitGame(") {
		t.Fatalf("player reply cpp lost the method body:\n%s", cpp)
	}
	assertGeneratedFiles(t, dirs.SceneNodeReplied)
}

func TestWriteMethodFileStopsNodeReplyRegistration(t *testing.T) {
	dirs := setupNodeRepliesGeneration(t)
	gate := nodeServiceMethods("proto/gate/gate_service.proto", messageoption.NodeType_NODE_GATE, "Gate", false, "NodeHandshake")
	scene := nodeServiceMethods("proto/scene/scene.proto", messageoption.NodeType_NODE_SCENE, "Scene", false, "NodeHandshake")
	internal.GlobalRPCServiceList = internal.RPCServiceInfoList{
		{ProtoFileInfo: gate[0].ProtoFileInfo, Methods: gate},
		{ProtoFileInfo: scene[0].ProtoFileInfo, Methods: scene},
	}

	var wg sync.WaitGroup
	WriteMethodFile(&wg)
	wg.Wait()

	// gate 的 rpc_replies 只剩生成的空 InitServiceHandler;两边都不再有 register_response_handler.cpp(InitReply)。
	assertGeneratedFiles(t, dirs.GateNodeReplied, "register_handler.cpp")
	assertGeneratedFiles(t, dirs.SceneNodeReplied)
	gateRegister := readGeneratedFile(t, dirs.GateNodeReplied+"register_handler.cpp")
	if !strings.Contains(gateRegister, "void InitServiceHandler()") || strings.Contains(gateRegister, "emplace") {
		t.Fatalf("gate rpc_replies/register_handler.cpp must stay an empty InitServiceHandler:\n%s", gateRegister)
	}

	// scene 的 handler/rpc/register_handler.cpp 照常注册 SceneHandler,不注册 Gate。
	sceneRegister := readGeneratedFile(t, dirs.SceneNode+"register_handler.cpp")
	if !strings.Contains(sceneRegister, `gNodeService.emplace("Scene", std::make_unique<SceneHandler>());`) || strings.Contains(sceneRegister, "GateHandler") {
		t.Fatalf("scene handler/rpc/register_handler.cpp changed:\n%s", sceneRegister)
	}
}
