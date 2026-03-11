package utils

import (
	"bytes"
	"sync"
	"text/template"

	"go.uber.org/zap"
	"pbgen/logger"
)

// TemplateEngine 模板引擎，简化模板操作
type TemplateEngine struct {
	mu    sync.RWMutex
	cache map[string]*template.Template
}

// NewTemplateEngine 创建模板引擎实例
func NewTemplateEngine() *TemplateEngine {
	return &TemplateEngine{
		cache: make(map[string]*template.Template),
	}
}

// 全局模板引擎实例
var GlobalEngine = NewTemplateEngine()

// Execute 执行模板并返回结果字符串
// name: 模板名称（用于缓存和错误日志）
// tmplStr: 模板字符串
// data: 模板数据
// funcs: 可选的模板函数映射
func (e *TemplateEngine) Execute(name, tmplStr string, data interface{}, funcs ...template.FuncMap) string {
	e.mu.RLock()
	tmpl, ok := e.cache[name]
	e.mu.RUnlock()
	if !ok {
		var err error
		t := template.New(name)
		// 合并所有 FuncMap
		for _, f := range funcs {
			t = t.Funcs(f)
		}
		tmpl, err = t.Parse(tmplStr)
		if err != nil {
			logger.Global.Fatal("解析模板失败",
				zap.String("template_name", name),
				zap.Error(err),
			)
		}
		e.mu.Lock()
		e.cache[name] = tmpl
		e.mu.Unlock()
	}

	var buf bytes.Buffer
	if err := tmpl.Execute(&buf, data); err != nil {
		logger.Global.Fatal("执行模板失败",
			zap.String("template_name", name),
			zap.Error(err),
		)
	}
	return buf.String()
}

// ExecuteTemplate 执行模板的便捷函数（使用全局引擎）
func ExecuteTemplate(name, tmplStr string, data interface{}, funcs ...template.FuncMap) string {
	return GlobalEngine.Execute(name, tmplStr, data, funcs...)
}

// MustExecute 执行模板，发生错误时返回错误而不是panic
func (e *TemplateEngine) MustExecute(name, tmplStr string, data interface{}, funcs ...template.FuncMap) (string, error) {
	t := template.New(name)
	for _, f := range funcs {
		t = t.Funcs(f)
	}
	tmpl, err := t.Parse(tmplStr)
	if err != nil {
		return "", err
	}

	var buf bytes.Buffer
	if err := tmpl.Execute(&buf, data); err != nil {
		return "", err
	}
	return buf.String(), nil
}
