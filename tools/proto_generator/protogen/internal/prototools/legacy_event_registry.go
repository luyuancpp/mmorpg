package prototools

import (
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"sync"

	"go.uber.org/zap"
	_config "protogen/internal/config"
	"protogen/internal/utils"
	"protogen/logger"
)

// RetireLegacyEventRegistries 清理已由直接 Kafka 路由和原生本地事件替代的注册汇总。
// 旧生成器停止写出文件还不够：既有检出里的汇总仍会调用已经删除的 Register 方法。
func RetireLegacyEventRegistries(wg *sync.WaitGroup) {
	wg.Add(1)
	defer wg.Done()
	if !_config.Global.Generators.EnableCpp {
		return
	}
	if err := retireLegacyEventRegistries(_config.Global.Paths.OutputRoot); err != nil {
		logger.Global.Fatal("清理旧事件注册汇总失败", zap.Error(err))
	}
}

func retireLegacyEventRegistries(outputRoot string) error {
	type update struct{ path, text string }
	var updates []update
	var retired []string
	for _, node := range []string{"scene", "gate"} {
		root := filepath.Join(outputRoot, "cpp", "nodes", node)
		for _, name := range []string{"CMakeLists.txt", node + ".vcxproj", node + ".vcxproj.filters"} {
			path := filepath.Join(root, name)
			original, err := os.ReadFile(path)
			if err != nil {
				return fmt.Errorf("读取 %s: %w", path, err)
			}
			text, err := removeLegacyEventRegistryEntries(name, string(original))
			if err != nil {
				return fmt.Errorf("%s: %w", path, err)
			}
			if text != string(original) {
				updates = append(updates, update{path, text})
			}
		}
		for _, name := range []string{"event_handler.cpp", "event_handler.h"} {
			retired = append(retired, filepath.Join(root, "handler", "event", name))
		}
	}
	// 所有清单通过校验后才开始发布，格式异常时保留全部原文件供人工诊断。
	for _, item := range updates {
		if err := utils.WriteFileIfChanged(item.path, []byte(item.text)); err != nil {
			return err
		}
	}
	for _, path := range retired {
		if err := os.Remove(path); err != nil && !os.IsNotExist(err) {
			return fmt.Errorf("删除旧汇总 %s: %w", path, err)
		}
	}
	return nil
}

func removeLegacyEventRegistryEntries(name, text string) (string, error) {
	file := `handler[/\\]event[/\\]event_handler\.(?:cpp|h)`
	if name == "CMakeLists.txt" {
		text = regexp.MustCompile(`(?m)^[ \t]*`+file+`[ \t]*(?:\r?\n|$)`).ReplaceAllString(text, "")
	} else {
		for _, item := range []string{"ClCompile", "ClInclude"} {
			entry := `(?m)^[ \t]*<` + item + ` Include="` + file + `"(?:[ \t]*/>|>[ \t\r\n]*(?:<Filter>[^<]*</Filter>[ \t\r\n]*)?</` + item + `>)[ \t]*(?:\r?\n|$)`
			text = regexp.MustCompile(entry).ReplaceAllString(text, "")
		}
	}
	if regexp.MustCompile(file).MatchString(text) {
		return "", fmt.Errorf("含未识别的旧事件汇总引用，拒绝清理")
	}
	return text, nil
}
