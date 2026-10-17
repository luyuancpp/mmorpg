package internal

import (
	"bufio"
	"fmt"
	"os"
	"sort"
	"strings"

	_config "protogen/internal/config"
)

// 命名守护段用于 RPC 的准入段(grpc.<M>.guard)与头文件声明，避免靠段落顺序识别代码。
// 沿用既有 BEGIN/END 标记，仅在 BEGIN 后追加名称；FirstCode 与 Handle* 的无名段仍由原解析器读取，
// 旧模板的无名投递前段只用于检测(checkLegacyGrpcSections)。
func readNamedGrpcCodeSections(path string) (map[string]string, error) {
	sections := make(map[string]string)
	file, err := os.Open(path)
	if os.IsNotExist(err) {
		return sections, nil
	}
	if err != nil {
		return nil, err
	}
	defer file.Close()

	prefix := _config.Global.Naming.YourCodeBegin + ":"
	var name string
	inSection := false
	var code strings.Builder
	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		line := scanner.Text()
		trimmed := strings.TrimSpace(line)
		if trimmed == _config.Global.Naming.YourCodeBegin || strings.HasPrefix(trimmed, prefix) {
			if inSection {
				return nil, fmt.Errorf("%s：守护段内不能嵌套守护段", path)
			}
			inSection = true
			if strings.HasPrefix(trimmed, prefix) {
				name = strings.TrimSpace(strings.TrimPrefix(trimmed, prefix))
				if name == "" {
					return nil, fmt.Errorf("%s：命名守护段缺少名称", path)
				}
				if _, exists := sections[name]; exists {
					return nil, fmt.Errorf("%s：守护段 %s 重复", path, name)
				}
				code.WriteString(line + "\n")
			}
			continue
		}
		if trimmed == _config.Global.Naming.YourCodeEnd {
			if !inSection {
				return nil, fmt.Errorf("%s：守护段结束标记没有对应开始标记", path)
			}
			if name != "" {
				code.WriteString(line + "\n")
				sections[name] = strings.TrimRight(code.String(), "\n")
			}
			name = ""
			inSection = false
			code.Reset()
			continue
		}
		if name != "" {
			code.WriteString(line + "\n")
		}
	}
	if err := scanner.Err(); err != nil {
		return nil, err
	}
	if inSection {
		return nil, fmt.Errorf("%s：守护段尚未闭合", path)
	}
	return sections, nil
}

// allowedNames 照常接受；emptyOnlyNames 是已退役的旧段名(如 grpc.<M>.in_loop / post_dispatch)，
// 只在为空时接受(isEmptyGrpcCodeSection)，由调用方丢弃；其余名字一律拒绝。
func validateNamedGrpcCodeSections(sections map[string]string, allowedNames []string, emptyOnlyNames []string) error {
	allowed := make(map[string]bool, len(allowedNames))
	for _, name := range allowedNames {
		allowed[name] = true
	}
	emptyOnly := make(map[string]bool, len(emptyOnlyNames))
	for _, name := range emptyOnlyNames {
		emptyOnly[name] = true
	}
	var unknown, nonEmptyLegacy []string
	for name, code := range sections {
		switch {
		case allowed[name]:
		case emptyOnly[name]:
			if !isEmptyGrpcCodeSection(code) {
				nonEmptyLegacy = append(nonEmptyLegacy, name)
			}
		default:
			unknown = append(unknown, name)
		}
	}
	if len(nonEmptyLegacy) > 0 {
		sort.Strings(nonEmptyLegacy)
		return fmt.Errorf("旧守护段非空，拒绝覆盖自定义代码(按 docs/design/grpc-muduo-transport.md §15.8.4 手工迁到 grpc.<M>.guard)：%s",
			strings.Join(nonEmptyLegacy, ", "))
	}
	if len(unknown) == 0 {
		return nil
	}
	sort.Strings(unknown)
	return fmt.Errorf("存在未识别的守护段，拒绝覆盖自定义代码：%s", strings.Join(unknown, ", "))
}

func namedGrpcCodeSection(sections map[string]string, name string, indent string) string {
	if code, exists := sections[name]; exists {
		return code
	}
	return indent + _config.Global.Naming.YourCodeBegin + ": " + name + "\n" +
		indent + _config.Global.Naming.YourCodeEnd
}
