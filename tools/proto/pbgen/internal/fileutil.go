package internal

import (
	"bytes"
	"log"
	"os"
	"text/template"
)

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

	// 读取现有的文件内容（如果文件存在）
	existingContent, err := os.ReadFile(outputPath)
	if err != nil && !os.IsNotExist(err) {
		// 如果读取文件失败且不是因为文件不存在，返回错误
		return err
	}

	// 如果文件存在且内容相同，则跳过写入
	if err == nil && bytes.Equal(existingContent, buf.Bytes()) {
		log.Println("文件内容没有变化，跳过写入:", outputPath)
		return nil
	}

	// 写入文件
	err = os.WriteFile(outputPath, buf.Bytes(), 0644)
	if err != nil {
		return err
	}

	log.Println("文件已更新:", outputPath)
	return nil
}
