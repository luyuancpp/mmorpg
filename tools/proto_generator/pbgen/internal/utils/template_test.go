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

	got := engine.Execute("normalize-spacing-exec", templateContent, nil)
	if strings.Contains(got, "\n\n\n") {
		t.Fatalf("expected at most one blank line between blocks, got: %q", got)
	}
	if !strings.Contains(got, "}\n\nfunc B()") {
		t.Fatalf("expected one blank line between functions, got: %q", got)
	}

	gotMust, err := engine.MustExecute("normalize-spacing-must", templateContent, nil)
	if err != nil {
		t.Fatalf("MustExecute returned error: %v", err)
	}
	if strings.Contains(gotMust, "\n\n\n") {
		t.Fatalf("expected at most one blank line between blocks, got: %q", gotMust)
	}
}

func TestTemplateEngineCachesByTemplateContent(t *testing.T) {
	engine := NewTemplateEngine()

	first := engine.Execute("same-name", `{{.Value}}`, map[string]string{"Value": "first"})
	second := engine.Execute("same-name", `prefix-{{.Value}}`, map[string]string{"Value": "second"})

	if first != "first" {
		t.Fatalf("unexpected first render: %q", first)
	}
	if second != "prefix-second" {
		t.Fatalf("unexpected second render: %q", second)
	}
}
