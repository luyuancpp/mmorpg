package utils

import (
	"bytes"
	"errors"
	"io"
	"io/fs"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"sync"
	"syscall"
	"text/template"
	"time"

	_config "protogen/internal/config"
	"protogen/logger"

	"go.uber.org/zap"
)

// Wg is the global WaitGroup for concurrent tasks.
var Wg sync.WaitGroup

func AreFileSizesEqual(file1, file2 string) bool {
	info1, err1 := os.Stat(file1)
	info2, err2 := os.Stat(file2)

	if err1 != nil || err2 != nil {
		return false
	}

	return info1.Size() == info2.Size()
}

func TrimTrailingLineBreaks(value string) string {
	return strings.TrimRight(value, "\r\n")
}

// WriteFileIfChanged writes content only if it differs from existing file.
func WriteFileIfChanged(outputPath string, content []byte) error {
	existingContent, err := os.ReadFile(outputPath)
	if err != nil && !os.IsNotExist(err) {
		return err
	}
	if err == nil && bytes.Equal(existingContent, content) {
		logger.Global.Info("File unchanged, skipping write",
			zap.String("file_path", outputPath),
		)
		return nil
	}
	if err = os.MkdirAll(filepath.Dir(outputPath), 0755); err != nil {
		return err
	}
	// 不能截断被gopls/编译器映射的旧绑定；同目录写全后替换，旧读者继续持有旧文件。
	staged, err := os.CreateTemp(filepath.Dir(outputPath), ".generated-*")
	if err != nil {
		return err
	}
	defer os.Remove(staged.Name())
	if _, err = staged.Write(content); err != nil {
		staged.Close()
		return err
	}
	if err = staged.Chmod(0644); err != nil {
		staged.Close()
		return err
	}
	if err = staged.Close(); err != nil {
		return err
	}
	// Go 1.27 的Root.Rename在Windows使用POSIX替换语义；os.Rename的旧API
	// 仍会拒绝存在内存映射读者的目标文件。根限定在同目录，不改读者或系统配置。
	root, err := os.OpenRoot(filepath.Dir(outputPath))
	if err != nil {
		return err
	}
	defer root.Close()
	// 索引器/杀毒扫描可能短暂禁止替换。仅对Windows共享/映射占用进行有界幂等重试；
	// 超过单调时钟预算仍返回原错误，不结束占用进程，也不截断旧文件。
	deadline := time.Now().Add(2 * time.Second)
	delay := 25 * time.Millisecond
	for {
		err = root.Rename(filepath.Base(staged.Name()), filepath.Base(outputPath))
		if err == nil {
			break
		}
		remaining := time.Until(deadline)
		if !isWindowsFileOccupancy(err) || remaining <= 0 {
			return err
		}
		if delay > remaining {
			delay = remaining
		}
		time.Sleep(delay)
		if delay < 200*time.Millisecond {
			delay *= 2
		}
	}
	logger.Global.Info("File updated",
		zap.String("file_path", outputPath),
	)
	return nil
}

func isWindowsFileOccupancy(err error) bool {
	// Win32:ACCESS_DENIED / SHARING_VIOLATION / LOCK_VIOLATION / USER_MAPPED_FILE。
	return runtime.GOOS == "windows" && (errors.Is(err, syscall.Errno(5)) || errors.Is(err, syscall.Errno(32)) ||
		errors.Is(err, syscall.Errno(33)) || errors.Is(err, syscall.Errno(1224)))
}

// WriteFileIfChangedSafe writes content when it changed and logs warnings instead of returning errors.
func WriteFileIfChangedSafe(filePath string, content []byte) {
	if err := WriteFileIfChanged(filePath, content); err != nil {
		logger.Global.Warn("Failed to write file",
			zap.String("file_path", filePath),
			zap.Error(err),
		)
	}
}

// RenderTemplateToFile renders a template and writes to file if content changed.
func RenderTemplateToFile(tmplPath string, outputPath string, data any) error {
	tmpl, err := template.ParseFiles(tmplPath)
	if err != nil {
		return err
	}
	var buf bytes.Buffer
	if err := tmpl.Execute(&buf, data); err != nil {
		return err
	}
	normalized := NormalizeGeneratedLayout(buf.String())
	return WriteFileIfChanged(outputPath, []byte(normalized))
}

func RenderTemplateToFileWithFuncs(tmplPath string, outputPath string, data any, funcMap template.FuncMap) error {
	tmpl := template.New(filepath.Base(tmplPath))
	if funcMap != nil {
		tmpl = tmpl.Funcs(funcMap)
	}
	tmpl, err := tmpl.ParseFiles(tmplPath)
	if err != nil {
		return err
	}
	var buf bytes.Buffer
	if err := tmpl.Execute(&buf, data); err != nil {
		return err
	}
	normalized := NormalizeGeneratedLayout(buf.String())
	WriteFileIfChangedSafe(outputPath, []byte(normalized))
	return nil
}

// CopyFileIfChangedAsync copies a file asynchronously if content differs.
func CopyFileIfChangedAsync(wg *sync.WaitGroup, inputPath, outputPath string) {
	wg.Add(1)
	go func() {
		defer wg.Done()
		srcContent, err := os.ReadFile(inputPath)
		if err != nil {
			logger.Global.Fatal("Failed to read source file",
				zap.String("source_path", inputPath),
				zap.Error(err),
			)
			return
		}
		dstContent, err := os.ReadFile(outputPath)
		if err != nil && !os.IsNotExist(err) {
			logger.Global.Fatal("Failed to read destination file",
				zap.String("dest_path", outputPath),
				zap.Error(err),
			)
			return
		}
		if err == nil && bytes.Equal(srcContent, dstContent) {
			return
		}
		if err := os.WriteFile(outputPath, srcContent, 0644); err != nil {
			logger.Global.Fatal("Failed to write destination file",
				zap.String("dest_path", outputPath),
				zap.Error(err),
			)
			return
		}
		logger.Global.Info("File copied",
			zap.String("dest_path", outputPath),
		)
	}()
}

// CopyFS copies all files from an fs.FS to the target directory.
func CopyFS(fsys fs.FS, targetDir string) error {
	return fs.WalkDir(fsys, ".", func(path string, d fs.DirEntry, err error) error {
		if err != nil {
			return err
		}
		targetPath := filepath.Join(targetDir, path)
		if d.IsDir() {
			info, err := d.Info()
			if err != nil {
				return err
			}
			return os.MkdirAll(targetPath, info.Mode()&0777)
		}
		return copyFSFile(fsys, path, targetPath)
	})
}

func copyFSFile(fsys fs.FS, srcPath, targetPath string) error {
	srcFile, err := fsys.Open(srcPath)
	if err != nil {
		return err
	}
	defer srcFile.Close()
	srcInfo, err := srcFile.Stat()
	if err != nil {
		return err
	}
	destFile, err := os.OpenFile(targetPath, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, srcInfo.Mode()&0777)
	if err != nil {
		return err
	}
	defer destFile.Close()
	_, err = io.Copy(destFile, srcFile)
	return err
}

// CopyLocalDir copies a local directory using CopyFS.
func CopyLocalDir(srcDir, targetDir string) error {
	return CopyFS(os.DirFS(srcDir), targetDir)
}

func GetLastTwoDirs(path string) string {
	cleanPath := filepath.Clean(path)
	parts := strings.Split(cleanPath, string(filepath.Separator))
	if len(parts) <= 2 {
		return cleanPath
	}
	return filepath.Join(parts[len(parts)-2], parts[len(parts)-1])
}

func IsProtoFile(fd os.DirEntry) bool {
	return !fd.IsDir() && filepath.Ext(fd.Name()) == ".proto"
}

func IncludeName(path string, protoName string) string {
	pbcHeadName := strings.Replace(protoName, _config.Global.FileExtensions.Proto, _config.Global.FileExtensions.PbH, 1)
	return _config.Global.Naming.IncludeBegin + strings.Replace(path, _config.Global.Paths.ProtoDir, "", 1) + pbcHeadName + "\"\n"
}

// GetBaseName extracts the base filename from a path, handling both / and \.
func GetBaseName(fullPath string) string {
	if fullPath == "" {
		return "."
	}
	path := strings.ReplaceAll(fullPath, "\\", "/")
	parts := strings.Split(path, "/")
	for i := len(parts) - 1; i >= 0; i-- {
		if parts[i] != "" {
			return parts[i]
		}
	}
	return "/"
}
