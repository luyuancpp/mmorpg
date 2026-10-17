//go:build windows

package utils

import (
	"os"
	"path/filepath"
	"syscall"
	"testing"
)

func TestChangedBindingCanReplaceMappedReader(t *testing.T) {
	path := filepath.Join(t.TempDir(), "guild.pb.go")
	if err := os.WriteFile(path, []byte("old binding"), 0644); err != nil {
		t.Fatal(err)
	}
	name, err := syscall.UTF16PtrFromString(path)
	if err != nil {
		t.Fatal(err)
	}
	handle, err := syscall.CreateFile(name, syscall.GENERIC_READ,
		syscall.FILE_SHARE_READ|syscall.FILE_SHARE_WRITE|syscall.FILE_SHARE_DELETE, nil, syscall.OPEN_EXISTING, 0, 0)
	if err != nil {
		t.Fatal(err)
	}
	reader := os.NewFile(uintptr(handle), path)
	defer reader.Close()
	mapping, err := syscall.CreateFileMapping(syscall.Handle(reader.Fd()), nil, syscall.PAGE_READONLY, 0, 0, nil)
	if err != nil {
		t.Fatal(err)
	}
	defer syscall.CloseHandle(mapping)
	view, err := syscall.MapViewOfFile(mapping, syscall.FILE_MAP_READ, 0, 0, 0)
	if err != nil {
		t.Fatal(err)
	}
	defer syscall.UnmapViewOfFile(view)
	// 映射读者允许文件替换：不能截断旧文件，旧映射应继续有效。
	if err = WriteFileIfChanged(path, []byte("new complete binding")); err != nil {
		t.Fatalf("存在映射读者时仍须安全发布新绑定: %v", err)
	}
	actual, err := os.ReadFile(path)
	if err != nil || string(actual) != "new complete binding" {
		t.Fatalf("绑定未完整发布: %q, %v", actual, err)
	}
}

func TestRefusedMappedBindingPublishKeepsOriginalBytes(t *testing.T) {
	path := filepath.Join(t.TempDir(), "guild.pb.go")
	if err := os.WriteFile(path, []byte("old complete binding"), 0644); err != nil {
		t.Fatal(err)
	}
	reader, err := os.Open(path) // Go普通读句柄不允许DELETE共享，不能强制夺走用户的句柄。
	if err != nil {
		t.Fatal(err)
	}
	defer reader.Close()
	mapping, err := syscall.CreateFileMapping(syscall.Handle(reader.Fd()), nil, syscall.PAGE_READONLY, 0, 0, nil)
	if err != nil {
		t.Fatal(err)
	}
	defer syscall.CloseHandle(mapping)
	view, err := syscall.MapViewOfFile(mapping, syscall.FILE_MAP_READ, 0, 0, 0)
	if err != nil {
		t.Fatal(err)
	}
	defer syscall.UnmapViewOfFile(view)
	if err = WriteFileIfChanged(path, []byte("new binding")); err == nil {
		t.Fatal("禁止替换的映射句柄必须让发布显式失败")
	}
	actual, err := os.ReadFile(path)
	if err != nil || string(actual) != "old complete binding" {
		t.Fatalf("失败不得截断原绑定: %q, %v", actual, err)
	}
}
