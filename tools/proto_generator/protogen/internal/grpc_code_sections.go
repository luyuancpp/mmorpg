package internal

import (
	"bufio"
	"fmt"
	"os"
	"sort"
	"strings"

	_config "protogen/internal/config"
)

// 命名守护段用于同一 RPC 的不同执行阶段与头文件声明，避免靠段落顺序识别代码。
// 沿用既有 BEGIN/END 标记，仅在 BEGIN 后追加名称；普通 Handle/投递前段仍由原解析器读取。
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

func validateNamedGrpcCodeSections(sections map[string]string, allowedNames []string) error {
	allowed := make(map[string]bool, len(allowedNames))
	for _, name := range allowedNames {
		allowed[name] = true
	}
	var unknown []string
	for name := range sections {
		if !allowed[name] {
			unknown = append(unknown, name)
		}
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
