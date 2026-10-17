package _go

import (
	"errors"
	"os"
	"path/filepath"
	"testing"
	"time"

	"go.uber.org/zap"
	"protogen/logger"
)

func TestUnifiedOutputPublishesOnlySuccessfulChangedBindings(t *testing.T) {
	previous := logger.Global
	logger.Global = zap.NewNop()
	t.Cleanup(func() { logger.Global = previous })
	parent := t.TempDir()
	output := filepath.Join(parent, "output")
	if err := os.MkdirAll(output, 0755); err != nil {
		t.Fatal(err)
	}
	stable := filepath.Join(output, "stable.pb.go")
	if err := os.WriteFile(stable, []byte("stable"), 0644); err != nil {
		t.Fatal(err)
	}
	stamp := time.Unix(1000, 0)
	if err := os.Chtimes(stable, stamp, stamp); err != nil {
		t.Fatal(err)
	}
	failure := errors.New("protoc 写出部分结果后失败")
	err := generateUnifiedOutput(output, parent, func(scratch string) error {
		if err := os.WriteFile(filepath.Join(scratch, "stable.pb.go"), []byte("partial"), 0644); err != nil {
			return err
		}
		return failure
	})
	if !errors.Is(err, failure) {
		t.Fatalf("没有保留生成失败: %v", err)
	}
	if actual, err := os.ReadFile(stable); err != nil || string(actual) != "stable" {
		t.Fatalf("生成失败污染原绑定: %q, %v", actual, err)
	}
	err = generateUnifiedOutput(output, parent, func(scratch string) error {
		if err := os.WriteFile(filepath.Join(scratch, "stable.pb.go"), []byte("stable"), 0644); err != nil {
			return err
		}
		if err := os.MkdirAll(filepath.Join(scratch, "guild"), 0755); err != nil {
			return err
		}
		return os.WriteFile(filepath.Join(scratch, "guild", "guild.pb.go"), []byte("updated"), 0644)
	})
	if err != nil {
		t.Fatal(err)
	}
	if actual, err := os.ReadFile(filepath.Join(output, "guild", "guild.pb.go")); err != nil || string(actual) != "updated" {
		t.Fatalf("未发布成功的新绑定: %q, %v", actual, err)
	}
	if info, err := os.Stat(stable); err != nil || !info.ModTime().Equal(stamp) {
		t.Fatalf("未变化绑定被重写: %v, %v", info, err)
	}
}

func TestGoPackageRewriteReleasesItsOwnReader(t *testing.T) {
	previous := logger.Global
	logger.Global = zap.NewNop()
	t.Cleanup(func() { logger.Global = previous })
	path := filepath.Join(t.TempDir(), "guild.proto")
	if err := os.WriteFile(path, []byte("syntax = \"proto3\";\noption go_package = \"old\";\n"), 0644); err != nil {
		t.Fatal(err)
	}
	changed, err := AddGoPackage(path, "proto/guild", false)
	if err != nil || !changed {
		t.Fatalf("生成器自己的读句柄不能阻塞发布: changed=%v err=%v", changed, err)
	}
	content, err := os.ReadFile(path)
	if err != nil || string(content) != "syntax = \"proto3\";\noption go_package = \"proto/guild\";\n" {
		t.Fatalf("契约未完整转换: %q, %v", content, err)
	}
}
