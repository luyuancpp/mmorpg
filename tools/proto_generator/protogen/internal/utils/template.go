package utils

import (
	"bytes"
	"hash/fnv"
	"strconv"
	"strings"
	"sync"
	"text/template"

	"protogen/logger"

	"go.uber.org/zap"
)

// TemplateEngine provides template caching and execution.
type TemplateEngine struct {
	mu    sync.RWMutex
	cache map[string]*template.Template
}

// NewTemplateEngine creates a new TemplateEngine instance.
func NewTemplateEngine() *TemplateEngine {
	return &TemplateEngine{
		cache: make(map[string]*template.Template),
	}
}

// GlobalEngine is the global template engine instance.
var GlobalEngine = NewTemplateEngine()

func templateCacheKey(name, tmplStr string) string {
	hasher := fnv.New64a()
	_, _ = hasher.Write([]byte(tmplStr))
	return name + ":" + strconv.FormatUint(hasher.Sum64(), 16)
}

// Execute renders a template and returns the result string.
// name: template name (used for caching and error logging)
// tmplStr: template string
// data: template data
// funcs: optional template function maps
func (e *TemplateEngine) Execute(name, tmplStr string, data interface{}, funcs ...template.FuncMap) string {
	cacheKey := templateCacheKey(name, tmplStr)
	e.mu.RLock()
	tmpl, ok := e.cache[cacheKey]
	e.mu.RUnlock()
	if !ok {
		var err error
		t := template.New(name)
		// Merge all FuncMaps
		for _, f := range funcs {
			t = t.Funcs(f)
		}
		tmpl, err = t.Parse(tmplStr)
		if err != nil {
			logger.Global.Fatal("Failed to parse template",
				zap.String("template_name", name),
				zap.Error(err),
			)
		}
		e.mu.Lock()
		e.cache[cacheKey] = tmpl
		e.mu.Unlock()
	}

	var buf bytes.Buffer
	if err := tmpl.Execute(&buf, data); err != nil {
		logger.Global.Fatal("Failed to execute template",
			zap.String("template_name", name),
			zap.Error(err),
		)
	}
	return NormalizeGeneratedLayout(buf.String())
}

// ExecuteTemplate is a convenience function using the global engine.
func ExecuteTemplate(name, tmplStr string, data interface{}, funcs ...template.FuncMap) string {
	return GlobalEngine.Execute(name, tmplStr, data, funcs...)
}

// MustExecute renders a template and returns an error instead of panicking.
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
	return NormalizeGeneratedLayout(buf.String()), nil
}

// NormalizeGeneratedLayout collapses blank lines (including whitespace-only lines)
// to keep generated files compact and stable for diffing.
func NormalizeGeneratedLayout(content string) string {
	if content == "" {
		return ""
	}

	lines := strings.Split(content, "\n")
	result := make([]string, 0, len(lines))
	prevBlank := false

	for _, line := range lines {
		if strings.TrimSpace(line) == "" {
			if prevBlank {
				continue
			}
			result = append(result, "")
			prevBlank = true
			continue
		}

		result = append(result, line)
		prevBlank = false
	}

	return strings.Join(result, "\n")
}
