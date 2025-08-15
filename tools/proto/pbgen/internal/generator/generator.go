package generator

import (
	"os"
	"path/filepath"
	"strings"
	"text/template"

	"pbgen/internal"
	"pbgen/internal/config"
)

// 使用 MethodInfo 渲染模板并生成文件
func GenerateFromMethod(info *internal.MethodInfo, tmplPath, outputPattern string) error {
	outputPath := strings.ReplaceAll(outputPattern, "{{proto_dir}}", info.Path())
	outputPath = strings.ReplaceAll(outputPath, "{{proto_file}}", info.FileBaseName())
	outputPath = strings.ReplaceAll(outputPath, "{{method_name}}", info.MethodName())
	outputPath = filepath.Join(config.OutputRoot, outputPath)

	// 保证目录存在
	os.MkdirAll(filepath.Dir(outputPath), 0755)

	// 读取模板
	content, err := os.ReadFile(tmplPath)
	if err != nil {
		return err
	}

	// 渲染
	tmpl, err := template.New("method").Parse(string(content))
	if err != nil {
		return err
	}

	f, err := os.Create(outputPath)
	if err != nil {
		return err
	}
	defer f.Close()

	// 传递 info 结构体给模板
	return tmpl.Execute(f, map[string]interface{}{
		"method":        info,
		"service_name":  info.Service(),
		"method_name":   info.MethodName(),
		"request_type":  info.RequestName(),
		"response_type": info.GoResponse(),
		"file_base":     info.FileBaseName(),
		"path":          info.Path(),
	})
}
