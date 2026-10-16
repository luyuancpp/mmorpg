package prototools

import (
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strings"
	"sync"

	_config "protogen/internal/config"
	"protogen/logger"

	"go.uber.org/zap"
)

// PruneOrphanedLocalEventOutputs 在事件号输入与墓碑门禁通过后运行,不因一次空输入提前删除产物。
// 这里只处理 common/event 的已退役契约,不会扫描或修改独立客户端仓库。
func PruneOrphanedLocalEventOutputs(wg *sync.WaitGroup) {
	wg.Add(1)
	defer wg.Done()
	if err := pruneOrphanedLocalEventOutputs(_config.Global.Paths.ProtoDir, _config.Global.Paths.OutputRoot); err != nil {
		logger.Global.Fatal("清理已退役的进程内事件产物失败", zap.Error(err))
	}
}

func pruneOrphanedLocalEventOutputs(protoRoot, outputRoot string) error {
	sourceDir := filepath.Join(protoRoot, "common", "event")
	if info, err := os.Stat(sourceDir); err != nil || !info.IsDir() {
		return fmt.Errorf("事件源目录不可用,拒绝清理: %s", sourceDir)
	}
	cppProtoRoot := filepath.Join(outputRoot, "cpp", "generated", "proto")
	metadataRoot := filepath.Join(outputRoot, "cpp", "generated", "rpc", "service_metadata")
	goEvents := filepath.Join(outputRoot, "go", "proto", "common", "event")
	stagingRoots := []string{
		filepath.Join(outputRoot, "generated", "proto", "db", "proto", "common", "event"),
		filepath.Join(outputRoot, "generated", "proto", "login", "proto", "common", "event"),
		filepath.Join(outputRoot, "generated", "proto", "_unified", "proto", "common", "event"),
		filepath.Join(outputRoot, "generated", "robot", "proto", "common", "event"),
	}
	cppEvents := filepath.Join(cppProtoRoot, "common", "event")
	candidates := map[string]bool{}
	for _, item := range []struct{ dir, prefix, suffix string }{
		{cppEvents, "", ".pb.h"}, {cppEvents, "", ".pb.cc"}, {goEvents, "", ".pb.go"},
		{metadataRoot, "common_event_", "_event_id.h"},
	} {
		if err := collectEventOutputStems(item.dir, item.prefix, item.suffix, candidates); err != nil {
			return err
		}
	}
	for _, dir := range stagingRoots {
		if err := collectEventOutputStems(dir, "", ".proto", candidates); err != nil {
			return err
		}
	}
	// 清单也保留删除依据:上次删完文件、写清单失败后,下一次仍能识别并移除陈旧条目。
	type projectUpdate struct {
		path, original, updated string
	}
	projects := make([]projectUpdate, 0, 3)
	projectEvent := regexp.MustCompile(`common[/\\]event[/\\]([A-Za-z0-9_]+_event)\.pb\.(?:cc|h)`)
	for _, name := range []string{"CMakeLists.txt", "proto.vcxproj", "proto.vcxproj.filters"} {
		path := filepath.Join(cppProtoRoot, name)
		data, err := os.ReadFile(path)
		if err != nil {
			return fmt.Errorf("读取事件工程清单 %s: %w", path, err)
		}
		for _, match := range projectEvent.FindAllStringSubmatch(string(data), -1) {
			candidates[match[1]] = true
		}
		projects = append(projects, projectUpdate{path: path, original: string(data)})
	}
	var retired []string
	for stem := range candidates {
		if _, err := os.Stat(filepath.Join(sourceDir, stem+".proto")); err == nil {
			continue
		} else if !os.IsNotExist(err) {
			return err
		}
		retired = append(retired, stem)
	}
	sort.Strings(retired)
	_, optionErr := os.Stat(filepath.Join(protoRoot, "common", "options", "event_ingress_option.proto"))
	if optionErr != nil && !os.IsNotExist(optionErr) {
		return optionErr
	}
	// 所有转换必须先成功,才允许删除文件或写出任何一份清单。
	for i := range projects {
		updated, err := syncLocalEventProject(projects[i].path, projects[i].original, retired, optionErr == nil)
		if err != nil {
			return err
		}
		projects[i].updated = updated
	}
	for _, stem := range retired {
		paths := []string{
			filepath.Join(cppEvents, stem+".pb.h"), filepath.Join(cppEvents, stem+".pb.cc"),
			filepath.Join(goEvents, stem+".pb.go"), filepath.Join(metadataRoot, "common_event_"+stem+"_event_id.h"),
		}
		for _, dir := range stagingRoots {
			paths = append(paths, filepath.Join(dir, stem+".proto"))
		}
		for _, path := range paths {
			if err := os.Remove(path); err != nil && !os.IsNotExist(err) {
				return fmt.Errorf("删除旧事件产物 %s: %w", path, err)
			}
		}
	}
	for _, project := range projects {
		if project.updated != project.original {
			if err := os.WriteFile(project.path, []byte(project.updated), 0644); err != nil {
				return fmt.Errorf("同步事件工程清单 %s: %w", project.path, err)
			}
		}
	}
	return nil
}

func collectEventOutputStems(dir, prefix, suffix string, stems map[string]bool) error {
	entries, err := os.ReadDir(dir)
	if os.IsNotExist(err) {
		return nil
	}
	if err != nil {
		return err
	}
	for _, entry := range entries {
		name := entry.Name()
		if !entry.IsDir() && strings.HasPrefix(name, prefix) && strings.HasSuffix(name, suffix) {
			stem := strings.TrimSuffix(strings.TrimPrefix(name, prefix), suffix)
			if strings.HasSuffix(stem, "_event") {
				stems[stem] = true
			}
		}
	}
	return nil
}

// 只计算转换结果,所有清单均通过校验后由调用方统一落盘。
func syncLocalEventProject(path, original string, retired []string, addIngressOption bool) (string, error) {
	updated := original
	cmake := filepath.Base(path) == "CMakeLists.txt"
	for _, stem := range retired {
		file := `common[/\\]event[/\\]` + regexp.QuoteMeta(stem) + `\.pb\.(?:cc|h)`
		if cmake {
			updated = regexp.MustCompile(`(?m)^[ \t]*`+file+`[ \t]*(?:\r?\n|$)`).ReplaceAllString(updated, "")
		} else {
			for _, item := range []string{"ClCompile", "ClInclude"} {
				updated = regexp.MustCompile(`(?m)^[ \t]*<`+item+` Include="`+file+`"(?:[ \t]*/>|>[ \t\r\n]*(?:<Filter>[^<]*</Filter>[ \t\r\n]*)?</`+item+`>)[ \t]*(?:\r?\n|$)`).ReplaceAllString(updated, "")
			}
		}
		if regexp.MustCompile(file).MatchString(updated) {
			return "", fmt.Errorf("%s 仍有未识别的旧事件条目 %s,拒绝清理", path, stem)
		}
	}
	if addIngressOption {
		kinds := []struct{ item, extension string }{{"ClCompile", "cc"}, {"ClInclude", "h"}}
		if cmake {
			kinds = kinds[:1]
		}
		for _, kind := range kinds {
			entryPattern := func(name string) *regexp.Regexp {
				file := `common[/\\]options[/\\]` + name + `\.pb\.` + kind.extension
				if cmake {
					return regexp.MustCompile(`(?m)^[ \t]*` + file + `[ \t]*(?:\r?\n|$)`)
				}
				return regexp.MustCompile(`(?m)^[ \t]*<` + kind.item + ` Include="` + file + `"(?:[ \t]*/>|>[ \t\r\n]*(?:<Filter>[^<]*</Filter>[ \t\r\n]*)?</` + kind.item + `>)[ \t]*(?:\r?\n|$)`)
			}
			newEntries := entryPattern("event_ingress_option").FindAllString(updated, -1)
			if len(newEntries) > 1 {
				return "", fmt.Errorf("%s 重复登记 event_ingress_option.pb.%s", path, kind.extension)
			}
			if len(newEntries) == 1 {
				continue
			}
			anchors := entryPattern("attribute_sync_option").FindAllString(updated, -1)
			if len(anchors) != 1 {
				return "", fmt.Errorf("%s 需要唯一 attribute_sync_option.pb.%s 登记以推导事件入口 option", path, kind.extension)
			}
			added := strings.ReplaceAll(anchors[0], "attribute_sync_option", "event_ingress_option")
			separator := ""
			if !strings.HasSuffix(anchors[0], "\n") {
				separator = "\n"
			}
			updated = strings.Replace(updated, anchors[0], anchors[0]+separator+added, 1)
		}
		optionRefs := regexp.MustCompile(`common[/\\]options[/\\]event_ingress_option\.pb\.(?:cc|h)`).FindAllString(updated, -1)
		if len(optionRefs) != len(kinds) {
			return "", fmt.Errorf("%s 的事件入口 option 登记不完整或含未识别条目", path)
		}
	}
	return updated, nil
}
