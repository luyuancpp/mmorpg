package cpp

import (
	"os"
	"path"
	"strings"
	"sync"

	"go.uber.org/zap" // 引入zap用于结构化日志字段

	"pbgen/internal"
	_config "pbgen/internal/config"
	utils2 "pbgen/internal/utils"
	"pbgen/logger" // 引入全局logger包
)

func GenNodeUtil(wg *sync.WaitGroup) {
	wg.Add(1)
	go func() {
		defer wg.Done()

		// 创建目标目录
		targetDir := path.Dir(_config.Global.Paths.GenUtilHeadFile)
		err := os.MkdirAll(targetDir, os.FileMode(0777))
		if err != nil {
			logger.Global.Fatal("生成节点工具类失败: 创建目录失败",
				zap.String("directory", targetDir),
				zap.Error(err),
			)
			return
		}

		// 收集节点列表
		nodeList := make([]string, 0, len(_config.Global.PathLists.ProtoDirectories))
		for _, file := range internal.FdSet.File {
			for _, enumDesc := range file.EnumType {
				if enumDesc.GetName() == _config.Global.Naming.NodeEnumName {
					for _, val := range enumDesc.Value {
						nodeName := strings.ReplaceAll(strings.ToLower(val.GetName()), _config.Global.Naming.NodeServiceSuffix, "")
						if !IsTcpNodeByEnum(nodeName) {
							continue
						}
						nodeList = append(nodeList, val.GetName())
					}
				}
			}
		}

		// 构建模板数据
		cppData := struct {
			NodeList []string
		}{
			NodeList: nodeList,
		}

		// 生成头文件
		if err := utils2.RenderTemplateToFile("internal/template/node_util.h.tmpl", _config.Global.Paths.GenUtilHeadFile, cppData); err != nil {
			logger.Global.Fatal("生成节点工具类失败: 渲染头文件模板失败",
				zap.String("file_path", _config.Global.Paths.GenUtilHeadFile),
				zap.String("template", "internal/template/node_util.h.tmpl"),
				zap.Error(err),
			)
		}

		// 生成cpp文件
		if err := utils2.RenderTemplateToFile("internal/template/node_util.cpp.tmpl", _config.Global.Paths.GenUtilCppFile, cppData); err != nil {
			logger.Global.Fatal("生成节点工具类失败: 渲染cpp文件模板失败",
				zap.String("file_path", _config.Global.Paths.GenUtilCppFile),
				zap.String("template", "internal/template/node_util.cpp.tmpl"),
				zap.Error(err),
			)
		}
	}()
}
