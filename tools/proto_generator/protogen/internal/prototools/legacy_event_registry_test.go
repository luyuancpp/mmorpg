package prototools

import (
	"os"
	"path/filepath"
	"strings"
	"testing"

	"go.uber.org/zap"
	"protogen/logger"
)

func legacyEventRegistryFixture(t *testing.T) string {
	t.Helper()
	previousLogger := logger.Global
	logger.Global = zap.NewNop()
	t.Cleanup(func() { logger.Global = previousLogger })
	root := t.TempDir()
	for _, node := range []string{"scene", "gate"} {
		dir := filepath.Join(root, "cpp", "nodes", node)
		if err := os.MkdirAll(filepath.Join(dir, "handler", "event"), 0755); err != nil {
			t.Fatal(err)
		}
		for name, text := range map[string]string{
			"CMakeLists.txt":                       "set(SOURCE_FILES\r\n    handler/event/event_handler.cpp\r\n    handler/local_event/native_events.cpp\r\n)\r\n",
			node + ".vcxproj":                      "<Project>\r\n  <ItemGroup>\r\n    <ClCompile Include=\"handler\\event\\event_handler.cpp\" />\r\n    <ClInclude Include=\"handler\\event\\event_handler.h\" />\r\n    <ClCompile Include=\"handler\\event\\team_event_handler.cpp\" />\r\n  </ItemGroup>\r\n</Project>\r\n",
			node + ".vcxproj.filters":              "<Project>\r\n  <ItemGroup>\r\n    <ClCompile Include=\"handler\\event\\event_handler.cpp\">\r\n      <Filter>源文件</Filter>\r\n    </ClCompile>\r\n    <ClInclude Include=\"handler/event/event_handler.h\" />\r\n  </ItemGroup>\r\n</Project>\r\n",
			"handler/event/event_handler.cpp":      "旧生成的汇总\n",
			"handler/event/event_handler.h":        "旧生成的汇总\n",
			"handler/event/team_event_handler.cpp": "保留 Kafka 业务处理函数\n",
		} {
			if err := os.WriteFile(filepath.Join(dir, filepath.FromSlash(name)), []byte(text), 0644); err != nil {
				t.Fatal(err)
			}
		}
	}
	return root
}

func TestRetireLegacyEventRegistriesRemovesOnlyObsoleteOutputs(t *testing.T) {
	root := legacyEventRegistryFixture(t)
	for pass := 0; pass < 2; pass++ {
		if err := retireLegacyEventRegistries(root); err != nil {
			t.Fatal(err)
		}
		for _, node := range []string{"scene", "gate"} {
			dir := filepath.Join(root, "cpp", "nodes", node)
			for _, name := range []string{"CMakeLists.txt", node + ".vcxproj", node + ".vcxproj.filters"} {
				text, err := os.ReadFile(filepath.Join(dir, name))
				if err != nil || strings.Contains(string(text), "event/event_handler.") || strings.Contains(string(text), `event\event_handler.`) {
					t.Fatalf("遗留汇总仍在构建中：%s err=%v\n%s", name, err, text)
				}
			}
			for _, ext := range []string{"cpp", "h"} {
				if _, err := os.Stat(filepath.Join(dir, "handler", "event", "event_handler."+ext)); !os.IsNotExist(err) {
					t.Fatalf("旧汇总仍存在：%s err=%v", ext, err)
				}
			}
			text, err := os.ReadFile(filepath.Join(dir, "handler", "event", "team_event_handler.cpp"))
			if err != nil || string(text) != "保留 Kafka 业务处理函数\n" {
				t.Fatal("修改了业务处理函数")
			}
		}
	}
}

func TestRetireLegacyEventRegistriesRejectsUnknownEntriesBeforeMutation(t *testing.T) {
	root := legacyEventRegistryFixture(t)
	broken := filepath.Join(root, "cpp", "nodes", "gate", "gate.vcxproj.filters")
	if err := os.WriteFile(broken, []byte(`<ClCompile Include="handler\event\event_handler.cpp"><Custom>保留</Custom></ClCompile>`), 0644); err != nil {
		t.Fatal(err)
	}
	if err := retireLegacyEventRegistries(root); err == nil {
		t.Fatal("未识别的自定义构建条目必须报错")
	}
	for _, node := range []string{"scene", "gate"} {
		dir := filepath.Join(root, "cpp", "nodes", node)
		if _, err := os.Stat(filepath.Join(dir, "handler", "event", "event_handler.cpp")); err != nil {
			t.Fatal("校验失败却删除原汇总")
		}
		text, err := os.ReadFile(filepath.Join(dir, "CMakeLists.txt"))
		if err != nil || !strings.Contains(string(text), "handler/event/event_handler.cpp") {
			t.Fatal("校验失败却发布半套清单")
		}
	}
}
