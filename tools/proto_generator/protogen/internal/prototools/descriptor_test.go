package prototools

import (
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"

	_config "protogen/internal/config"
)

func writeDescriptorTestFile(t *testing.T, path string) {
	t.Helper()
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(path, []byte("syntax = \"proto3\";\n"), 0o644); err != nil {
		t.Fatal(err)
	}
}

func descriptorSlashPath(parts ...string) string {
	return filepath.ToSlash(filepath.Join(parts...))
}

func TestCollectDomainProtoFilesReturnsAbsoluteSlashPathsIncludingSubdirs(t *testing.T) {
	root := t.TempDir()
	writeDescriptorTestFile(t, filepath.Join(root, "chat.proto"))
	writeDescriptorTestFile(t, filepath.Join(root, "sub", "chat_extra.proto"))
	writeDescriptorTestFile(t, filepath.Join(root, "README.md"))

	files, err := collectDomainProtoFiles("chat", root)
	if err != nil {
		t.Fatal(err)
	}
	want := []string{descriptorSlashPath(root, "chat.proto"), descriptorSlashPath(root, "sub", "chat_extra.proto")}
	if !reflect.DeepEqual(files, want) {
		t.Fatalf("files = %v, want %v", files, want)
	}
	for _, f := range files {
		if strings.Contains(f, "\\") || !filepath.IsAbs(filepath.FromSlash(f)) {
			t.Fatalf("应为 / 分隔的绝对路径: %s", f)
		}
	}
}

// 回归:域的 source 目录不存在(proto_gen*.yaml 里曾配着已删的 slg 域)以前只打 WARN 继续,
// 该域的 RPC 方法会被永久转成 message_id 墓碑;现在必须报错,并写明域名与路径。
func TestCollectDomainProtoFilesRejectsMissingSource(t *testing.T) {
	missing := filepath.Join(t.TempDir(), "slg")

	files, err := collectDomainProtoFiles("slg", missing)
	if err == nil {
		t.Fatalf("source 不存在应报错,得到 files = %v", files)
	}
	for _, want := range []string{"domain_meta.slg", missing} {
		if !strings.Contains(err.Error(), want) {
			t.Fatalf("错误信息缺少 %q: %v", want, err)
		}
	}
}

func TestCollectDomainProtoFilesRejectsDirectoryWithoutProto(t *testing.T) {
	root := t.TempDir()
	writeDescriptorTestFile(t, filepath.Join(root, "README.md"))
	if err := os.MkdirAll(filepath.Join(root, "empty_sub"), 0o755); err != nil {
		t.Fatal(err)
	}

	_, err := collectDomainProtoFiles("db", root)
	if err == nil {
		t.Fatal("走下来 0 个 .proto 应报错")
	}
	for _, want := range []string{"domain_meta.db", root, "no .proto"} {
		if !strings.Contains(err.Error(), want) {
			t.Fatalf("错误信息缺少 %q: %v", want, err)
		}
	}
}

func TestCollectDomainProtoFilesRejectsEmptySourceAndFileSource(t *testing.T) {
	if _, err := collectDomainProtoFiles("gate", ""); err == nil || !strings.Contains(err.Error(), "domain_meta.gate") {
		t.Fatalf("空 source 应报错并写明域名: %v", err)
	}

	file := filepath.Join(t.TempDir(), "gate.proto")
	writeDescriptorTestFile(t, file)
	_, err := collectDomainProtoFiles("gate", file)
	if err == nil {
		t.Fatal("source 指向文件应报错")
	}
	for _, want := range []string{"domain_meta.gate", file, "not a directory"} {
		if !strings.Contains(err.Error(), want) {
			t.Fatalf("错误信息缺少 %q: %v", want, err)
		}
	}
}

// 方法数不超过墓碑上限的小域(db 1 个方法)整域缺失时,整轮收集必须失败,不能带着其余域的文件继续。
func TestCollectUniqueProtoFilesFailsWhenAnyDomainIsUnreadable(t *testing.T) {
	root := t.TempDir()
	writeDescriptorTestFile(t, filepath.Join(root, "chat", "chat.proto"))
	missingDb := filepath.Join(root, "db")

	files, err := collectUniqueProtoFiles(map[string]_config.DomainMeta{
		"chat": {Source: filepath.Join(root, "chat")},
		"db":   {Source: missingDb},
	})
	if err == nil {
		t.Fatalf("有域读不到时应整轮失败,得到 files = %v", files)
	}
	if files != nil {
		t.Fatalf("失败时不应返回部分结果: %v", files)
	}
	for _, want := range []string{"domain_meta.db", missingDb} {
		if !strings.Contains(err.Error(), want) {
			t.Fatalf("错误信息缺少 %q: %v", want, err)
		}
	}
}

// 源目录重叠(父域包含子域目录)时去重,输出按路径排序,与 map 遍历顺序无关。
func TestCollectUniqueProtoFilesDeduplicatesAndSorts(t *testing.T) {
	root := t.TempDir()
	writeDescriptorTestFile(t, filepath.Join(root, "b", "b.proto"))
	writeDescriptorTestFile(t, filepath.Join(root, "a", "a.proto"))
	domains := map[string]_config.DomainMeta{
		"all": {Source: root},
		"b":   {Source: filepath.Join(root, "b")},
		"a":   {Source: filepath.Join(root, "a")},
	}

	want := []string{descriptorSlashPath(root, "a", "a.proto"), descriptorSlashPath(root, "b", "b.proto")}
	for i := 0; i < 5; i++ {
		files, err := collectUniqueProtoFiles(domains)
		if err != nil {
			t.Fatal(err)
		}
		if !reflect.DeepEqual(files, want) {
			t.Fatalf("files = %v, want %v", files, want)
		}
	}
}
