package prototools

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func writeCleanupFixture(t *testing.T, root, relative, content string) string {
	t.Helper()
	path := filepath.Join(root, filepath.FromSlash(relative))
	if err := os.MkdirAll(filepath.Dir(path), 0755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(path, []byte(content), 0644); err != nil {
		t.Fatal(err)
	}
	return path
}

func cleanupProjectFixtures() map[string]string {
	return map[string]string{
		"CMakeLists.txt":        "    common/event/actor_event.pb.cc\n    common/event/battle_event.pb.cc\n    common/options/attribute_sync_option.pb.cc\n",
		"proto.vcxproj":         "<Project>\n  <ClCompile Include=\"common\\event\\actor_event.pb.cc\" />\n  <ClInclude Include=\"common\\event\\actor_event.pb.h\" />\n  <ClCompile Include=\"common\\event\\battle_event.pb.cc\" />\n  <ClCompile Include=\"common\\options\\attribute_sync_option.pb.cc\" />\n  <ClInclude Include=\"common\\options\\attribute_sync_option.pb.h\" />\n</Project>\n",
		"proto.vcxproj.filters": "<Project>\n  <ClCompile Include=\"common\\event\\actor_event.pb.cc\">\n    <Filter>common</Filter>\n  </ClCompile>\n  <ClCompile Include=\"common\\event\\battle_event.pb.cc\">\n    <Filter>common</Filter>\n  </ClCompile>\n  <ClCompile Include=\"common\\options\\attribute_sync_option.pb.cc\">\n    <Filter>common</Filter>\n  </ClCompile>\n  <ClInclude Include=\"common\\options\\attribute_sync_option.pb.h\">\n    <Filter>common</Filter>\n  </ClInclude>\n</Project>\n",
	}
}

func writeCleanupProjects(t *testing.T, root string) {
	t.Helper()
	for name, fixture := range cleanupProjectFixtures() {
		writeCleanupFixture(t, root, "cpp/generated/proto/"+name, fixture)
	}
}

func TestPruneLocalEventsKeepsLiveContractsAndRemovesAllOwnedCopies(t *testing.T) {
	root := t.TempDir()
	writeCleanupProjects(t, root)
	writeCleanupFixture(t, root, "proto/common/event/battle_event.proto", "syntax = \"proto3\";")
	writeCleanupFixture(t, root, "cpp/generated/proto/common/event/battle_event.pb.h", "live")
	writeCleanupFixture(t, root, "go/proto/common/event/battle_event.pb.go", "live")
	writeCleanupFixture(t, root, "cpp/generated/proto/other/actor_event.pb.h", "unrelated")
	var removed []string
	for _, relative := range []string{
		"cpp/generated/proto/common/event/actor_event.pb.h",
		"cpp/generated/proto/common/event/actor_event.pb.cc",
		"cpp/generated/rpc/service_metadata/common_event_actor_event_event_id.h",
		"go/proto/common/event/actor_event.pb.go",
		"generated/proto/db/proto/common/event/actor_event.proto",
		"generated/proto/login/proto/common/event/actor_event.proto",
		"generated/proto/_unified/proto/common/event/actor_event.proto",
		"generated/robot/proto/common/event/actor_event.proto",
	} {
		removed = append(removed, writeCleanupFixture(t, root, relative, "stale"))
	}
	if err := pruneOrphanedLocalEventOutputs(filepath.Join(root, "proto"), root); err != nil {
		t.Fatal(err)
	}
	for _, path := range removed {
		if _, err := os.Stat(path); !os.IsNotExist(err) {
			t.Errorf("旧副本未移除: %s, err=%v", path, err)
		}
	}
	for _, relative := range []string{"cpp/generated/proto/common/event/battle_event.pb.h", "go/proto/common/event/battle_event.pb.go", "cpp/generated/proto/other/actor_event.pb.h"} {
		if _, err := os.Stat(filepath.Join(root, filepath.FromSlash(relative))); err != nil {
			t.Errorf("不应删除: %s, err=%v", relative, err)
		}
	}
}

func TestPruneLocalEventsMissingSourceDirectoryFailsBeforeDeletion(t *testing.T) {
	root := t.TempDir()
	path := writeCleanupFixture(t, root, "cpp/generated/proto/common/event/actor_event.pb.h", "keep")
	if err := pruneOrphanedLocalEventOutputs(filepath.Join(root, "missing-proto"), root); err == nil {
		t.Fatal("源目录缺失必须拒绝清理")
	}
	if _, err := os.Stat(path); err != nil {
		t.Fatal("拒绝后不应有删除副作用:", err)
	}
}

func TestSyncLocalEventProjectsDropsRetiredEntriesAndAddsIngressOption(t *testing.T) {
	for name, fixture := range cleanupProjectFixtures() {
		first, err := syncLocalEventProject(name, fixture, []string{"actor_event"}, true)
		if err != nil {
			t.Fatal(err)
		}
		if strings.Contains(first, "actor_event.pb.") || !strings.Contains(first, "event_ingress_option.pb.cc") {
			t.Errorf("清单未正确同步 %s: %s", name, first)
		}
		if name != "CMakeLists.txt" && !strings.Contains(first, "event_ingress_option.pb.h") {
			t.Errorf("缺少 option 头登记: %s", name)
		}
		second, err := syncLocalEventProject(name, first, []string{"actor_event"}, true)
		if err != nil {
			t.Fatal(err)
		}
		if first != second {
			t.Errorf("第二次同步不应再修改 %s", name)
		}
	}
}

func TestSyncLocalEventProjectWithoutFinalNewlineIsIdempotent(t *testing.T) {
	first, err := syncLocalEventProject("CMakeLists.txt", "  common/event/actor_event.pb.cc\n  common/options/attribute_sync_option.pb.cc", []string{"actor_event"}, true)
	if err != nil {
		t.Fatal(err)
	}
	second, err := syncLocalEventProject("CMakeLists.txt", first, []string{"actor_event"}, true)
	if err != nil || first != second || strings.Contains(first, "actor_event.pb.cc") {
		t.Fatalf("无末尾换行时也应稳定移除/追加完整行: first=%q second=%q err=%v", first, second, err)
	}
}

func TestPruneLocalEventsRecoversWhenOnlyStaleProjectEntriesRemain(t *testing.T) {
	root := t.TempDir()
	writeCleanupProjects(t, root)
	writeCleanupFixture(t, root, "proto/common/event/battle_event.proto", "live")
	writeCleanupFixture(t, root, "proto/common/options/event_ingress_option.proto", "option")
	for run := 0; run < 2; run++ {
		if err := pruneOrphanedLocalEventOutputs(filepath.Join(root, "proto"), root); err != nil {
			t.Fatal(err)
		}
		for name := range cleanupProjectFixtures() {
			data, err := os.ReadFile(filepath.Join(root, "cpp", "generated", "proto", name))
			if err != nil || strings.Contains(string(data), "actor_event.pb.") ||
				!strings.Contains(string(data), "battle_event.pb.cc") ||
				!strings.Contains(string(data), "event_ingress_option.pb.cc") {
				t.Fatalf("第 %d 轮清单未恢复: %s, err=%v, data=%s", run, name, err, data)
			}
		}
	}
}

func TestPruneLocalEventsPreflightsEveryProjectBeforeSideEffects(t *testing.T) {
	for _, failure := range []string{"缺清单", "旧项格式未知", "option头锚点缺失", "旧项XML未闭合"} {
		t.Run(failure, func(t *testing.T) {
			root := t.TempDir()
			writeCleanupProjects(t, root)
			writeCleanupFixture(t, root, "proto/common/event/battle_event.proto", "live")
			writeCleanupFixture(t, root, "proto/common/options/event_ingress_option.proto", "option")
			output := writeCleanupFixture(t, root, "cpp/generated/proto/common/event/actor_event.pb.h", "keep")
			broken := filepath.Join(root, "cpp", "generated", "proto", "proto.vcxproj.filters")
			switch failure {
			case "缺清单":
				if err := os.Remove(broken); err != nil {
					t.Fatal(err)
				}
			case "旧项格式未知":
				writeCleanupFixture(t, root, "cpp/generated/proto/proto.vcxproj.filters", "<Project>\n<ClCompile Include='common/event/actor_event.pb.cc' />\n</Project>\n")
			case "option头锚点缺失":
				fixture := cleanupProjectFixtures()["proto.vcxproj.filters"]
				fixture = strings.ReplaceAll(fixture, "attribute_sync_option.pb.h", "unknown_option.pb.h")
				writeCleanupFixture(t, root, "cpp/generated/proto/proto.vcxproj.filters", fixture)
			case "旧项XML未闭合":
				writeCleanupFixture(t, root, "cpp/generated/proto/proto.vcxproj.filters", "<Project>\n  <ClCompile Include=\"common/event/actor_event.pb.cc\">\n  <ClCompile Include=\"common/event/battle_event.pb.cc\">\n    <Filter>common</Filter>\n  </ClCompile>\n</Project>\n")
			}
			before := make(map[string]string)
			for name := range cleanupProjectFixtures() {
				path := filepath.Join(root, "cpp", "generated", "proto", name)
				if data, err := os.ReadFile(path); err == nil {
					before[path] = string(data)
				}
			}
			if err := pruneOrphanedLocalEventOutputs(filepath.Join(root, "proto"), root); err == nil {
				t.Fatal("不完整清单应拒绝全部清理")
			}
			if _, err := os.Stat(output); err != nil {
				t.Fatal("校验失败不应删除产物:", err)
			}
			for path, original := range before {
				if data, err := os.ReadFile(path); err != nil || string(data) != original {
					t.Fatalf("校验失败不应修改其他清单: %s err=%v", path, err)
				}
			}
		})
	}
}
