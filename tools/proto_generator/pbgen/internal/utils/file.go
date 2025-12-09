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

	"pbgen/logger" // 引入封装的logger包
	_config "pbgen/internal/config"
	"go.uber.org/zap" // 引入zap用于结构化日志字段
)

func AreFileSizesEqual(file1, file2 string) bool {
	info1, err1 := os.Stat(file1)
	info2, err2 := os.Stat(file2)

	if err1 != nil || err2 != nil {
		return false
	}

	return info1.Size() == info2.Size()
}

// WriteFileIfChanged 写入内容到文件，若内容未变化则不写入
func WriteFileIfChanged(outputPath string, content []byte) error {
	// 读取现有文件内容
	existingContent, err := os.ReadFile(outputPath)
	if err != nil && !os.IsNotExist(err) {
		return err
	}

	// 若内容相同，跳过写入
	if err == nil && bytes.Equal(existingContent, content) {
		logger.Global.Info("文件内容无变化，跳过写入",
			zap.String("文件路径", outputPath),
		)
		return nil
	}

	// 写入新内容
	err = os.WriteFile(outputPath, content, 0644)
	if err != nil {
		return err
	}

	logger.Global.Info("文件已更新",
		zap.String("文件路径", outputPath),
	)
	return nil
}

// WriteFileIfChangedSafe 新函数
// 写入内容到文件，内容未变化则不写入；文件不存在则创建；内部处理错误，不返回错误码
func WriteFileIfChangedSafe(filePath string, content []byte) {
	// 读取现有文件内容
	existingContent, err := os.ReadFile(filePath)
	// 仅处理“非文件不存在”的读取错误
	// 场景2：文件不存在（读取失败且是“文件不存在”错误）
	if os.IsNotExist(err) {
		logger.Global.Info("文件不存在，开始创建",
			zap.String("文件路径", filePath),
		)

		// 先创建文件所在的多级目录
		dir := filepath.Dir(filePath)
		if mkdirErr := os.MkdirAll(dir, 0755); mkdirErr != nil {
			logger.Global.Fatal("创建目录失败",
				zap.String("目录路径", dir),
				zap.Error(mkdirErr),
			)
		}

		// 写入内容创建文件
		if writeErr := os.WriteFile(filePath, content, 0644); writeErr != nil {
			logger.Global.Fatal("创建文件失败",
				zap.String("文件路径", filePath),
				zap.Error(writeErr),
			)
		}

		logger.Global.Info("文件创建成功",
			zap.String("文件路径", filePath),
		)
		return // 返回刚写入的内容
	}

	// 内容相同则跳过写入
	if err == nil && bytes.Equal(existingContent, content) {
		logger.Global.Info("文件内容无变化，跳过写入",
			zap.String("文件路径", filePath),
		)
		return
	}

	// 写入新内容（文件不存在时自动创建）
	err = os.WriteFile(filePath, content, 0644)
	if err != nil {
		logger.Global.Warn("写入文件失败",
			zap.String("文件路径", filePath),
			zap.Error(err),
		)
		return
	}

	logger.Global.Info("文件已更新/创建",
		zap.String("文件路径", filePath),
	)
}

// ===================== 原有函数（保持不变，兼容旧调用） =====================
// RenderTemplateToFile 渲染模板并写入文件，如果文件内容未改变则不写入
// （原版本，无自定义函数参数）
func RenderTemplateToFile(tmplPath string, outputPath string, data any) error {
	// 解析模板
	tmpl, err := template.ParseFiles(tmplPath)
	if err != nil {
		return err
	}

	// 渲染模板到缓冲区
	var buf bytes.Buffer
	if err := tmpl.Execute(&buf, data); err != nil {
		return err
	}

	// 使用统一写入逻辑
	return WriteFileIfChanged(outputPath, buf.Bytes())
}

func RenderTemplateToFileWithFuncs(tmplPath string, outputPath string, data any, funcMap template.FuncMap) error {
	// 1. 创建模板对象
	tmpl := template.New(filepath.Base(tmplPath))

	// 2. 注册自定义函数（如果有）
	if funcMap != nil {
		tmpl = tmpl.Funcs(funcMap)
	}

	// 3. 解析模板文件
	tmpl, err := tmpl.ParseFiles(tmplPath)
	if err != nil {
		return err
	}

	// 4. 渲染模板到缓冲区
	var buf bytes.Buffer
	if err := tmpl.Execute(&buf, data); err != nil {
		return err
	}

	// 5. 统一写入逻辑（内容未变则不写入）
	WriteFileIfChangedSafe(outputPath, buf.Bytes())

	return nil
}

// CopyFileIfChanged 如果源文件内容与目标文件不同，则复制源文件到目标文件
func CopyFileIfChangedAsync(wg *sync.WaitGroup, inputPath, outputPath string) {
	wg.Add(1)
	go func() {
		defer wg.Done()

		// 读取源文件内容
		srcContent, err := os.ReadFile(inputPath)
		if err != nil {
			logger.Global.Fatal("读取源文件失败",
				zap.String("源文件路径", inputPath),
				zap.Error(err),
			)
			return
		}

		// 读取目标文件内容（如果存在）
		dstContent, err := os.ReadFile(outputPath)
		if err != nil && !os.IsNotExist(err) {
			logger.Global.Fatal("读取目标文件失败",
				zap.String("目标文件路径", outputPath),
				zap.Error(err),
			)
			return
		}

		// 如果文件存在且内容相同，跳过写入
		if err == nil && bytes.Equal(srcContent, dstContent) {
			// 文件未变化，跳过
			return
		}

		// 写入目标文件
		if err := os.WriteFile(outputPath, srcContent, 0644); err != nil {
			logger.Global.Fatal("写入目标文件失败",
				zap.String("目标文件路径", outputPath),
				zap.Error(err),
			)
			return
		}

		logger.Global.Info("文件已复制/更新",
			zap.String("目标文件路径", outputPath),
		)
	}()
}

// CopyFS 将fs.FS中的所有内容拷贝到目标目录
// fsys: 源文件系统（可以是os.DirFS、embed.FS等）
// targetDir: 目标目录（本地文件系统路径）
// 特点：递归拷贝所有文件和目录，覆盖已存在文件
func CopyFS(fsys fs.FS, targetDir string) error {
	// 遍历源文件系统中的所有内容
	return fs.WalkDir(fsys, ".", func(path string, d fs.DirEntry, err error) error {
		if err != nil {
			return err
		}

		// 构建目标路径
		targetPath := filepath.Join(targetDir, path)

		if d.IsDir() {
			// 创建目标目录（包含权限）
			info, err := d.Info()
			if err != nil {
				return err
			}
			return os.MkdirAll(targetPath, info.Mode()&0777)
		}

		// 拷贝文件内容
		return copyFSFile(fsys, path, targetPath)
	})
}

// copyFSFile 拷贝fs.FS中的单个文件到本地文件系统
func copyFSFile(fsys fs.FS, srcPath, targetPath string) error {
	// 打开源文件（来自fs.FS）
	srcFile, err := fsys.Open(srcPath)
	if err != nil {
		return err
	}
	defer srcFile.Close()

	// 获取源文件信息（用于权限设置）
	srcInfo, err := srcFile.Stat()
	if err != nil {
		return err
	}

	// 创建目标文件（覆盖已存在文件）
	destFile, err := os.OpenFile(targetPath, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, srcInfo.Mode()&0777)
	if err != nil {
		return err
	}
	defer destFile.Close()

	// 拷贝文件内容
	_, err = io.Copy(destFile, srcFile)
	return err
}

// 便捷函数：拷贝本地目录（基于os.DirFS）
func CopyLocalDir(srcDir, targetDir string) error {
	// 将本地目录包装为fs.FS接口
	fsys := os.DirFS(srcDir)
	return CopyFS(fsys, targetDir)
}

func GetLastTwoDirs(path string) string {
	// 先清理路径（处理相对路径、重复分隔符等）
	cleanPath := filepath.Clean(path)

	// 分割为组件
	parts := strings.Split(cleanPath, string(filepath.Separator))
	length := len(parts)

	// 处理边界情况
	if length <= 1 {
		return cleanPath
	} else if length == 2 {
		return cleanPath
	}

	// 取最后两个组件
	return filepath.Join(parts[length-2], parts[length-1])
}

func IsProtoFile(fd os.DirEntry) bool {
	if fd.IsDir() || filepath.Ext(fd.Name()) != ".proto" {
		return false
	}
	return true
}

func IncludeName(path string, protoName string) string {
	pbcHeadName := strings.Replace(protoName, _config.Global.FileExtensions.Proto, _config.Global.FileExtensions.PbH, 1)
	return _config.Global.Naming.IncludeBegin + strings.Replace(path, _config.Global.Paths.ProtoDir, "", 1) + pbcHeadName + "\"\n"
}

// GetBaseName 从文件路径中提取基础文件名（不含路径部分）
// 例如：
// - 输入 "/a/b/c.txt" → 返回 "c.txt"
// - 输入 "d:/x/y/z.proto" → 返回 "z.proto"
// - 输入 "/" → 返回 "/"
// - 输入 "" 或 nil → 返回 "."
func GetBaseName(fullPath string) string {
	if fullPath == "" {
		return "."
	}

	// 处理路径分隔符（同时兼容 / 和 \）
	// 先将 \ 统一替换为 /，便于后续处理
	path := strings.ReplaceAll(fullPath, "\\", "/")

	// 按 / 分割路径
	parts := strings.Split(path, "/")
	var base string

	// 从后往前找最后一个非空部分（处理 trailing / 的情况，如 "a/b/c/" → "c"）
	for i := len(parts) - 1; i >= 0; i-- {
		if parts[i] != "" {
			base = parts[i]
			break
		}
	}

	// 边界情况处理
	if base == "" {
		// 全路径都是分隔符（如 "/" 或 "//"）
		return "/"
	}

	return base
}
