package cpp

import (
	"os"
	"path"
	"strings"
	"sync"

	"go.uber.org/zap"

	"pbgen/internal"
	_config "pbgen/internal/config"
	utils2 "pbgen/internal/utils"
	"pbgen/logger"
)

func GenNodeUtil(wg *sync.WaitGroup) {
	wg.Add(1)
	go func() {
		defer wg.Done()

		targetDir := path.Dir(_config.Global.Paths.GenUtilHeadFile)
		err := os.MkdirAll(targetDir, os.FileMode(0777))
		if err != nil {
			logger.Global.Fatal("生成节点工具类失败: 创建目录失败",
				zap.String("directory", targetDir),
				zap.Error(err),
			)
			return
		}

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

		cppData := struct {
			NodeList []string
		}{
			NodeList: nodeList,
		}

		if err := utils2.RenderTemplateToFile("internal/template/node_util.h.tmpl", _config.Global.Paths.GenUtilHeadFile, cppData); err != nil {
			logger.Global.Fatal("生成节点工具类失败: 渲染头文件模板失败",
				zap.String("file_path", _config.Global.Paths.GenUtilHeadFile),
				zap.String("template", "internal/template/node_util.h.tmpl"),
				zap.Error(err),
			)
		}

		if err := utils2.RenderTemplateToFile("internal/template/node_util.cpp.tmpl", _config.Global.Paths.GenUtilCppFile, cppData); err != nil {
			logger.Global.Fatal("生成节点工具类失败: 渲染cpp文件模板失败",
				zap.String("file_path", _config.Global.Paths.GenUtilCppFile),
				zap.String("template", "internal/template/node_util.cpp.tmpl"),
				zap.Error(err),
			)
		}
	}()
}
