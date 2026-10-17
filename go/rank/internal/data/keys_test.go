package data

import (
	"go/ast"
	"go/parser"
	"go/token"
	"strings"
	"testing"
)

// keyConstructors 是 keys.go 里**全部**键构造函数(T9)。新增构造函数必须加进来,否则 TestKeyConstructorTableIsComplete 变红。
func keyConstructors() map[string]func() string {
	return map[string]func() string{
		"RatingScoresKey":   RatingScoresKey,
		"RatingVersionsKey": RatingVersionsKey,
	}
}

// TestKeysStayUnderRootPrefix(T9):每个键都在 lb: 根前缀下、带 {rating} hash tag,且不会落进 go/guild RebuildRanks
// 会扫掉的 guild_rank* 或 match 的 match:* 命名空间。
//
// 变异验证(Codex 做一次):临时把 RootPrefix 改成 "guild_rank:x",本用例必须变红,再改回。
func TestKeysStayUnderRootPrefix(t *testing.T) {
	for name, build := range keyConstructors() {
		k := build()
		if !strings.HasPrefix(k, RootPrefix) {
			t.Errorf("%s() = %q 不在根前缀 %q 下", name, k, RootPrefix)
		}
		if strings.HasPrefix(k, "guild_rank") {
			t.Errorf("%s() = %q 以 guild_rank 开头,会被公会 RebuildRanks 删掉", name, k)
		}
		if strings.HasPrefix(k, "match:") {
			t.Errorf("%s() = %q 落进了 match 的命名空间", name, k)
		}
		if !strings.Contains(k, "{rating}") {
			t.Errorf("%s() = %q 没有 {rating} hash tag:写入 Lua 同时碰 ZSET 与版本表,两者必须同 slot", name, k)
		}
	}
	if RatingScoresKey() == RatingVersionsKey() {
		t.Fatal("ZSET 与版本表不能是同一个键")
	}
}

// TestKeyConstructorTableIsComplete:按 AST 数 keys.go 里返回 string 的顶层函数,必须与 keyConstructors 表一一对应
// (表长 == 构造函数个数,且名字相同)。加了新构造函数不进表 = T9 对它不设防。
func TestKeyConstructorTableIsComplete(t *testing.T) {
	fset := token.NewFileSet()
	f, err := parser.ParseFile(fset, "keys.go", nil, 0)
	if err != nil {
		t.Fatalf("解析 keys.go 失败: %v", err)
	}
	found := map[string]bool{}
	for _, decl := range f.Decls {
		fn, ok := decl.(*ast.FuncDecl)
		if !ok || fn.Recv != nil || fn.Type.Results == nil || len(fn.Type.Results.List) != 1 {
			continue
		}
		if ident, ok := fn.Type.Results.List[0].Type.(*ast.Ident); ok && ident.Name == "string" {
			found[fn.Name.Name] = true
		}
	}
	table := keyConstructors()
	if len(found) != len(table) {
		t.Fatalf("keys.go 有 %d 个键构造函数 %v,表里登记了 %d 个", len(found), found, len(table))
	}
	for name := range found {
		if _, ok := table[name]; !ok {
			t.Errorf("keys.go 的 %s 没有登记进 keyConstructors", name)
		}
	}
}
