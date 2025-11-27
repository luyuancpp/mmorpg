package utils

import (
	"bytes"
	"fmt"
	"io"
	"io/fs"
	"log"
	"os"
	"path/filepath"
	"pbgen/config"
	_config "pbgen/internal/config"
	"strings"
	"text/template"
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
		log.Println("文件内容没有变化，跳过写入:", outputPath)
		return nil
	}

	// 写入新内容
	err = os.WriteFile(outputPath, content, 0644)
	if err != nil {
		return err
	}

	log.Println("文件已更新:", outputPath)
	return nil
}

// RenderTemplateToFile 渲染模板并写入文件，如果文件内容未改变则不写入
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

// CopyFileIfChanged 如果源文件内容与目标文件不同，则复制源文件到目标文件
func CopyFileIfChanged(inputPath, outputPath string) error {
	// 读取源文件内容
	srcContent, err := os.ReadFile(inputPath)
	if err != nil {
		return fmt.Errorf("读取源文件失败: %w", err)
	}

	// 读取目标文件内容（如果存在）
	dstContent, err := os.ReadFile(outputPath)
	if err != nil && !os.IsNotExist(err) {
		return fmt.Errorf("读取目标文件失败: %w", err)
	}

	// 如果文件存在且内容相同，跳过写入
	if err == nil && bytes.Equal(srcContent, dstContent) {
		//log.Println("文件内容未变化，跳过写入:", outputPath)
		return nil
	}

	// 写入目标文件
	if err := os.WriteFile(outputPath, srcContent, 0644); err != nil {
		return fmt.Errorf("写入目标文件失败: %w", err)
	}

	log.Println("文件已复制/更新:", outputPath)
	return nil
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
	pbcHeadName := strings.Replace(protoName, config.ProtoExt, _config.Global.FileExtensions.PbH, 1)
	return config.IncludeBegin + strings.Replace(path, _config.Global.Paths.ProtoDir, "", 1) + pbcHeadName + "\"\n"
}
