package utils

import (
	"bytes"
	"hash/fnv"
	"regexp"
	"strconv"
	"strings"
	"sync"
	"text/template"

	"pbgen/logger"

	"go.uber.org/zap"
)

var excessiveBlankLinesPattern = regexp.MustCompile(`\n{3,}`)

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

func templateCacheKey(name, tmplStr string) string {
	hasher := fnv.New64a()
	_, _ = hasher.Write([]byte(tmplStr))
	return name + ":" + strconv.FormatUint(hasher.Sum64(), 16)
}

// Execute renders a template string with the given data, caching parsed templates by name.
func (e *TemplateEngine) Execute(name, tmplStr string, data interface{}, funcs ...template.FuncMap) string {
	cacheKey := templateCacheKey(name, tmplStr)
	e.mu.RLock()
	tmpl, ok := e.cache[cacheKey]
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
		e.cache[cacheKey] = tmpl
		e.mu.Unlock()
	}

	var buf bytes.Buffer
	if err := tmpl.Execute(&buf, data); err != nil {
		logger.Global.Fatal("执行模板失败",
			zap.String("template_name", name),
			zap.Error(err),
		)
	}
	return NormalizeGeneratedLayout(buf.String())
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
	return NormalizeGeneratedLayout(buf.String()), nil
}

// NormalizeGeneratedLayout keeps generated output compact and stable.
func NormalizeGeneratedLayout(content string) string {
	return normalizeGeneratedLayout(content)
}

func normalizeGeneratedLayout(content string) string {
	if content == "" {
		return content
	}

	hasCRLF := strings.Contains(content, "\r\n")
	normalized := strings.ReplaceAll(content, "\r\n", "\n")
	normalized = strings.TrimLeft(normalized, "\n")
	normalized = excessiveBlankLinesPattern.ReplaceAllString(normalized, "\n\n")

	if hasCRLF {
		normalized = strings.ReplaceAll(normalized, "\n", "\r\n")
	}
	return normalized
}
