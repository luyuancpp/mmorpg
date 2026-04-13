package utils

import (
	"os"
	"path/filepath"
	"sort"
	"sync"
	"testing"
	"testing/fstest"

	"protogen/logger"
)

func TestMain(m *testing.M) {
	if err := logger.Init(); err != nil {
		panic(err)
	}
	defer logger.Sync()
	os.Exit(m.Run())
}

func TestAreFileSizesEqual(t *testing.T) {
	dir := t.TempDir()
	fileA := filepath.Join(dir, "a.txt")
	fileB := filepath.Join(dir, "b.txt")
	fileC := filepath.Join(dir, "c.txt")

	if err := os.WriteFile(fileA, []byte("abcd"), 0644); err != nil {
		t.Fatalf("write fileA failed: %v", err)
	}
	if err := os.WriteFile(fileB, []byte("wxyz"), 0644); err != nil {
		t.Fatalf("write fileB failed: %v", err)
	}
	if err := os.WriteFile(fileC, []byte("toolong"), 0644); err != nil {
		t.Fatalf("write fileC failed: %v", err)
	}

	if !AreFileSizesEqual(fileA, fileB) {
		t.Fatalf("expected same file sizes")
	}
	if AreFileSizesEqual(fileA, fileC) {
		t.Fatalf("expected different file sizes")
	}
	if AreFileSizesEqual(fileA, filepath.Join(dir, "missing.txt")) {
		t.Fatalf("expected false when file does not exist")
	}
}

func TestWriteFileIfChanged(t *testing.T) {
	dir := t.TempDir()
	file := filepath.Join(dir, "content.txt")

	if err := WriteFileIfChanged(file, []byte("first")); err != nil {
		t.Fatalf("first write failed: %v", err)
	}

	if err := WriteFileIfChanged(file, []byte("first")); err != nil {
		t.Fatalf("second write failed: %v", err)
	}

	if err := WriteFileIfChanged(file, []byte("second")); err != nil {
		t.Fatalf("third write failed: %v", err)
	}

	got, err := os.ReadFile(file)
	if err != nil {
		t.Fatalf("read result failed: %v", err)
	}
	if string(got) != "second" {
		t.Fatalf("unexpected file content: %q", string(got))
	}
}

func TestCollectProtoFiles(t *testing.T) {
	dir := t.TempDir()
	nested := filepath.Join(dir, "nested")
	if err := os.MkdirAll(nested, 0755); err != nil {
		t.Fatalf("mkdir failed: %v", err)
	}

	paths := []string{
		filepath.Join(dir, "a.proto"),
		filepath.Join(nested, "b.proto"),
		filepath.Join(nested, "ignore.txt"),
	}
	for _, p := range paths {
		if err := os.WriteFile(p, []byte("x"), 0644); err != nil {
			t.Fatalf("write %s failed: %v", p, err)
		}
	}

	got, err := CollectProtoFiles(dir)
	if err != nil {
		t.Fatalf("CollectProtoFiles failed: %v", err)
	}

	if len(got) != 2 {
		t.Fatalf("expected 2 proto files, got %d: %v", len(got), got)
	}

	sort.Strings(got)
	for _, p := range got {
		if !filepath.IsAbs(p) {
			t.Fatalf("expected absolute path, got %q", p)
		}
		if filepath.Ext(p) != ".proto" {
			t.Fatalf("expected .proto file, got %q", p)
		}
	}
}

func TestCopyFS(t *testing.T) {
	fakeFS := fstest.MapFS{
		"a/b.proto": {Data: []byte("proto-data"), Mode: 0644},
		"a/c.txt":   {Data: []byte("txt-data"), Mode: 0644},
	}

	target := t.TempDir()
	if err := CopyFS(fakeFS, target); err != nil {
		t.Fatalf("CopyFS failed: %v", err)
	}

	content, err := os.ReadFile(filepath.Join(target, "a", "b.proto"))
	if err != nil {
		t.Fatalf("read copied file failed: %v", err)
	}
	if string(content) != "proto-data" {
		t.Fatalf("unexpected copied content: %q", string(content))
	}
}

func TestCopyFileIfChangedAsync(t *testing.T) {
	dir := t.TempDir()
	src := filepath.Join(dir, "src.proto")
	dst := filepath.Join(dir, "dst.proto")

	if err := os.WriteFile(src, []byte("v1"), 0644); err != nil {
		t.Fatalf("write source failed: %v", err)
	}

	var wg sync.WaitGroup
	CopyFileIfChangedAsync(&wg, src, dst)
	wg.Wait()

	got, err := os.ReadFile(dst)
	if err != nil {
		t.Fatalf("read destination failed: %v", err)
	}
	if string(got) != "v1" {
		t.Fatalf("unexpected destination content: %q", string(got))
	}
}

func TestPathHelpers(t *testing.T) {
	if got := GetLastTwoDirs(filepath.Join("a", "b", "c")); got != filepath.Join("b", "c") {
		t.Fatalf("GetLastTwoDirs mismatch: %q", got)
	}

	if got := GetBaseName("d:/x/y/z.proto"); got != "z.proto" {
		t.Fatalf("GetBaseName mismatch for absolute path: %q", got)
	}
	if got := GetBaseName("a/b/c/"); got != "c" {
		t.Fatalf("GetBaseName mismatch for trailing slash: %q", got)
	}
}
