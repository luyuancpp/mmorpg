package internal

import (
	"bytes"
	"os"
	"text/template"
)

// renderTemplateToFile 渲染模板并写入文件
func renderTemplateToFile(tmplPath string, outputPath string, data any) error {
	tmpl, err := template.ParseFiles(tmplPath)
	if err != nil {
		return err
	}

	var buf bytes.Buffer
	if err := tmpl.Execute(&buf, data); err != nil {
		return err
	}

	return os.WriteFile(outputPath, buf.Bytes(), 0644)
}
