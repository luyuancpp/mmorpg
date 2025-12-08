package tpl

import (
	"os"
	"text/template"
)

// ExecuteTemplateToFile 将指定模板渲染到文件
func ExecuteTemplateToFile(templateName string, filePath string, data interface{}) error {
	// 假设你的模板已经通过 template.ParseFiles 或 ParseGlob 解析过
	tmpl, err := template.ParseFiles(templateName) // 可以根据实际模板目录调整
	if err != nil {
		return err
	}

	// 创建目标文件
	f, err := os.Create(filePath)
	if err != nil {
		return err
	}
	defer f.Close()

	// 渲染模板到文件
	return tmpl.Execute(f, data)
}
