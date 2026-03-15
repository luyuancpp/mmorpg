package utils

import (
	"bytes"
	"io"
	"io/fs"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"text/template"

	_config "pbgen/internal/config"
	"pbgen/logger"

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
		logger.Global.Info("文件内容无变化，跳过写入",
			zap.String("文件路径", outputPath),
		)
		return nil
	}
	if err = os.MkdirAll(filepath.Dir(outputPath), 0755); err != nil {
		return err
	}
	if err = os.WriteFile(outputPath, content, 0644); err != nil {
		return err
	}
	logger.Global.Info("文件已更新",
		zap.String("文件路径", outputPath),
	)
	return nil
}

// WriteFileIfChangedSafe writes content when it changed and logs warnings instead of returning errors.
func WriteFileIfChangedSafe(filePath string, content []byte) {
	if err := WriteFileIfChanged(filePath, content); err != nil {
		logger.Global.Warn("写入文件失败",
			zap.String("文件路径", filePath),
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
			logger.Global.Fatal("读取源文件失败",
				zap.String("源文件路径", inputPath),
				zap.Error(err),
			)
			return
		}
		dstContent, err := os.ReadFile(outputPath)
		if err != nil && !os.IsNotExist(err) {
			logger.Global.Fatal("读取目标文件失败",
				zap.String("目标文件路径", outputPath),
				zap.Error(err),
			)
			return
		}
		if err == nil && bytes.Equal(srcContent, dstContent) {
			return
		}
		if err := os.WriteFile(outputPath, srcContent, 0644); err != nil {
			logger.Global.Fatal("写入目标文件失败",
				zap.String("目标文件路径", outputPath),
				zap.Error(err),
			)
			return
		}
		logger.Global.Info("文件已复制",
			zap.String("目标文件路径", outputPath),
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
