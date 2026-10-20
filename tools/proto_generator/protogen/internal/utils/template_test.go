package utils

import (
	"strings"
	"testing"
)

func TestTemplateEngineNormalizesFunctionSpacing(t *testing.T) {
	engine := NewTemplateEngine()
	templateContent := `func A() {
}



func B() {
}
`

	got := engine.RenderOrExit("normalize-spacing-render-or-exit", templateContent, nil)
	if strings.Contains(got, "\n\n\n") {
		t.Fatalf("expected at most one blank line between blocks, got: %q", got)
	}
	if !strings.Contains(got, "}\n\nfunc B()") {
		t.Fatalf("expected one blank line between functions, got: %q", got)
	}

	rendered, err := engine.Render("normalize-spacing-render", templateContent, nil)
	if err != nil {
		t.Fatalf("Render 返回错误: %v", err)
	}
	if strings.Contains(rendered, "\n\n\n") {
		t.Fatalf("expected at most one blank line between blocks, got: %q", rendered)
	}
}

func TestTemplateEngineCachesByTemplateContent(t *testing.T) {
	engine := NewTemplateEngine()

	first := engine.RenderOrExit("same-name", `{{.Value}}`, map[string]string{"Value": "first"})
	second := engine.RenderOrExit("same-name", `prefix-{{.Value}}`, map[string]string{"Value": "second"})

	if first != "first" {
		t.Fatalf("unexpected first render: %q", first)
	}
	if second != "prefix-second" {
		t.Fatalf("unexpected second render: %q", second)
	}
}

func TestNormalizeGeneratedLayoutCollapsesIndentedBlankLines(t *testing.T) {
	input := "func A() {\n}\n\t\n    \n\nfunc B() {\n}\n"
	got := NormalizeGeneratedLayout(input)

	if strings.Contains(got, "\n\n\n") {
		t.Fatalf("expected no double blank lines, got: %q", got)
	}
	if !strings.Contains(got, "}\n\nfunc B()") {
		t.Fatalf("expected exactly one blank line between functions, got: %q", got)
	}
}
