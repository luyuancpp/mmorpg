package main

// team 与 logic 的分层护栏(docs/design/team-system.md §A.2):两个包互不 import,唯一的类型级接缝是
// team.BattleStarter,由本包 main() 注入 logic.TeamBattleStarter。这条边界是"team 寄宿在 match 进程、
// 将来可整体拆出"的前提(team-system.md「定案理由复核与扩展路径」):一旦有人顺手让 team 直接调 logic
// (或反过来),拆分就从"换掉一个端口实现"变成"解开一张依赖网"。测试文件一并检查。
// 只拦直接 import:目前两包依赖的 svc / metrics / playercontract 等都不反向引用它们,没有间接路径;
// 将来若出现 team → 中间包 → logic,需要把本测试扩成沿 match/internal/... 的传递闭包检查。

import (
	"go/parser"
	"go/token"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"testing"
)

const (
	teamPackageDir  = "internal/team"
	logicPackageDir = "internal/logic"
	teamImportPath  = "match/internal/team"
	logicImportPath = "match/internal/logic"
)

// importsOf 返回 dir 下每个 .go 文件(含 _test.go)的 import 路径,键为文件名。
func importsOf(t *testing.T, dir string) map[string][]string {
	t.Helper()
	entries, err := os.ReadDir(dir)
	if err != nil {
		t.Fatalf("读目录 %s 失败: %v", dir, err)
	}
	out := make(map[string][]string)
	fset := token.NewFileSet()
	for _, entry := range entries {
		if entry.IsDir() || !strings.HasSuffix(entry.Name(), ".go") {
			continue
		}
		path := filepath.Join(dir, entry.Name())
		file, err := parser.ParseFile(fset, path, nil, parser.ImportsOnly)
		if err != nil {
			t.Fatalf("解析 %s 失败: %v", path, err)
		}
		for _, spec := range file.Imports {
			importPath, err := strconv.Unquote(spec.Path.Value)
			if err != nil {
				t.Fatalf("%s 的 import %s 无法解析: %v", path, spec.Path.Value, err)
			}
			out[entry.Name()] = append(out[entry.Name()], importPath)
		}
	}
	if len(out) == 0 {
		t.Fatalf("%s 下没有任何 .go 文件,目录可能已改名,护栏失效", dir)
	}
	return out
}

// importsPackage path 是否就是 pkg 或其子包。
func importsPackage(path, pkg string) bool {
	return path == pkg || strings.HasPrefix(path, pkg+"/")
}

func TestTeamAndLogicDoNotImportEachOther(t *testing.T) {
	cases := []struct {
		dir       string
		forbidden string
	}{
		{teamPackageDir, logicImportPath},
		{logicPackageDir, teamImportPath},
	}
	for _, c := range cases {
		for file, imports := range importsOf(t, c.dir) {
			for _, importPath := range imports {
				if importsPackage(importPath, c.forbidden) {
					t.Errorf("%s/%s import 了 %s:team 与 logic 只能经 team.BattleStarter 端口相接,由 main() 注入",
						c.dir, file, importPath)
				}
			}
		}
	}
}
