package prototools

import (
	"fmt"
	"io/fs"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strings"
	"sync"

	"go.uber.org/zap"
	_config "protogen/internal/config"
	"protogen/internal/utils"
	"protogen/logger"
)

// SyncCppBuildProjects 让新生成的协议和调用适配器同时进入 Windows 与 Linux 构建。
// 只补当前源契约仍存在的产物，不删除手工配置、特殊编译选项或其他条目。
func SyncCppBuildProjects(wg *sync.WaitGroup) {
	wg.Add(1)
	defer wg.Done()
	if !_config.Global.Generators.EnableCpp {
		return
	}
	if err := syncCppBuildProjects(_config.Global.Paths.ProtoDir, _config.Global.Paths.OutputRoot); err != nil {
		logger.Global.Fatal("同步 C++ 生成工程失败", zap.Error(err))
	}
}

func syncCppBuildProjects(protoRoot, outputRoot string) error {
	type update struct{ path, text string }
	var updates []update
	for _, project := range []string{"proto", "grpc_client"} {
		root := filepath.Join(outputRoot, "cpp", "generated", project)
		var sources, headers []string
		err := filepath.WalkDir(root, func(path string, entry fs.DirEntry, err error) error {
			if err != nil {
				return err
			}
			if entry.IsDir() {
				return nil
			}
			rel, err := filepath.Rel(root, path)
			if err != nil {
				return err
			}
			rel = filepath.ToSlash(rel)
			if project == "proto" {
				if !strings.HasSuffix(rel, ".pb.cc") && !strings.HasSuffix(rel, ".pb.h") {
					return nil
				}
				stem := strings.TrimSuffix(strings.TrimSuffix(rel, ".cc"), ".h")
				stem = strings.TrimSuffix(strings.TrimSuffix(stem, ".pb"), ".grpc")
				if _, err := os.Stat(filepath.Join(protoRoot, filepath.FromSlash(stem+".proto"))); os.IsNotExist(err) {
					return nil
				} else if err != nil {
					return err
				}
			} else if !strings.HasSuffix(rel, "_grpc_client.cpp") && !strings.HasSuffix(rel, "_grpc_client.h") {
				return nil
			}
			if strings.HasSuffix(rel, ".h") {
				headers = append(headers, rel)
			} else {
				sources = append(sources, rel)
			}
			return nil
		})
		if err != nil {
			return err
		}
		sort.Strings(sources)
		sort.Strings(headers)
		for _, name := range []string{"CMakeLists.txt", project + ".vcxproj", project + ".vcxproj.filters"} {
			path := filepath.Join(root, name)
			original, err := os.ReadFile(path)
			if err != nil {
				return err
			}
			text, err := appendCppProjectEntries(name, string(original), sources, headers)
			if err != nil {
				return fmt.Errorf("%s: %w", path, err)
			}
			if text != string(original) {
				updates = append(updates, update{path, text})
			}
		}
	}
	// 所有清单先校验成功，再写入；不因某个清单形状错误只完成半套登记。
	for _, item := range updates {
		if err := utils.WriteFileIfChanged(item.path, []byte(item.text)); err != nil {
			return err
		}
	}
	return nil
}

func appendCppProjectEntries(name, text string, sources, headers []string) (string, error) {
	newline := "\n"
	if strings.Contains(text, "\r\n") {
		newline = "\r\n"
	}
	if name == "CMakeLists.txt" {
		anchor := regexp.MustCompile(`(?ms)^set\(SOURCE_FILES[ \t]*\r?\n(.*?)^\)`)
		matches := anchor.FindAllStringSubmatchIndex(text, -1)
		if len(matches) != 1 {
			return "", fmt.Errorf("SOURCE_FILES 必须恰好一段")
		}
		match := matches[0]
		existing := map[string]bool{}
		for _, line := range strings.Split(text[match[2]:match[3]], "\n") {
			existing[strings.TrimSpace(line)] = true
		}
		var lines []string
		for _, source := range sources {
			if !existing[source] {
				lines = append(lines, "    "+source)
			}
		}
		if len(lines) == 0 {
			return text, nil
		}
		return text[:match[3]] + strings.Join(lines, newline) + newline + text[match[3]:], nil
	}
	if strings.Count(text, "</Project>") != 1 {
		return "", fmt.Errorf("Project 结束标签必须恰好一个")
	}
	var additions []string
	for _, group := range []struct {
		tag, filter string
		files       []string
	}{{"ClCompile", "源文件", sources}, {"ClInclude", "头文件", headers}} {
		existing := map[string]bool{}
		pattern := regexp.MustCompile(`<` + group.tag + `\s+Include="([^"]+)"`)
		for _, match := range pattern.FindAllStringSubmatch(text, -1) {
			existing[strings.ReplaceAll(match[1], "\\", "/")] = true
		}
		for _, file := range group.files {
			if existing[file] {
				continue
			}
			file = strings.ReplaceAll(file, "/", "\\")
			if strings.HasSuffix(name, ".filters") {
				additions = append(additions, fmt.Sprintf("    <%s Include=\"%s\">%s      <Filter>%s</Filter>%s    </%s>", group.tag, file, newline, group.filter, newline, group.tag))
			} else {
				additions = append(additions, fmt.Sprintf("    <%s Include=\"%s\" />", group.tag, file))
			}
		}
	}
	if len(additions) == 0 {
		return text, nil
	}
	block := "  <ItemGroup Label=\"ProtoGeneratorSources\">" + newline + strings.Join(additions, newline) + newline + "  </ItemGroup>" + newline
	return strings.Replace(text, "</Project>", block+"</Project>", 1), nil
}
