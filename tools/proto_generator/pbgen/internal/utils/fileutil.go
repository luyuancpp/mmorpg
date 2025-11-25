package utils

import (
	"bytes"
	"fmt"
	"log"
	"os"
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
