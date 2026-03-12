package utils

import (
	"bytes"
	"sync"
	"text/template"

	"go.uber.org/zap"
	"pbgen/logger"
)

// TemplateEngine is a thread-safe template cache and executor.
type TemplateEngine struct {
	mu    sync.RWMutex
	cache map[string]*template.Template
}

// NewTemplateEngine creates a new TemplateEngine.
func NewTemplateEngine() *TemplateEngine {
	return &TemplateEngine{
		cache: make(map[string]*template.Template),
	}
}

var GlobalEngine = NewTemplateEngine()

// Execute renders a template string with the given data, caching parsed templates by name.
func (e *TemplateEngine) Execute(name, tmplStr string, data interface{}, funcs ...template.FuncMap) string {
	e.mu.RLock()
	tmpl, ok := e.cache[name]
	e.mu.RUnlock()
	if !ok {
		var err error
		t := template.New(name)
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

// ExecuteTemplate is a convenience wrapper using GlobalEngine.
func ExecuteTemplate(name, tmplStr string, data interface{}, funcs ...template.FuncMap) string {
	return GlobalEngine.Execute(name, tmplStr, data, funcs...)
}

// MustExecute renders a template and returns an error instead of calling Fatal.
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
