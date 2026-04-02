package cpp

import (
	"os"
	"path"
	"strings"
	"sync"

	"go.uber.org/zap"

	"protogen/internal"
	_config "protogen/internal/config"
	utils2 "protogen/internal/utils"
	"protogen/logger"
)

func GenNodeUtil(wg *sync.WaitGroup) {
	wg.Add(1)
	go func() {
		defer wg.Done()

		targetDir := path.Dir(_config.Global.Paths.GenUtilHeadFile)
		err := os.MkdirAll(targetDir, os.FileMode(0777))
		if err != nil {
			logger.Global.Fatal("Failed to generate node util: directory creation failed",
				zap.String("directory", targetDir),
				zap.Error(err),
			)
			return
		}

		nodeList := make([]string, 0, len(_config.Global.PathLists.ProtoDirectories))
		nodeEnumCppQualified := _config.Global.Naming.NodeEnumName // fallback: bare name
		for _, file := range internal.FdSet.File {
			for _, enumDesc := range file.EnumType {
				if enumDesc.GetName() == _config.Global.Naming.NodeEnumName {
					// Build C++ qualified enum type from the proto package
					pkg := file.GetPackage()
					if pkg != "" {
						cppPkg := strings.ReplaceAll(pkg, ".", "::")
						nodeEnumCppQualified = cppPkg + "::" + _config.Global.Naming.NodeEnumName
					}
					for _, val := range enumDesc.Value {
						internal.NodeEnumValueSet[val.GetName()] = true
						nodeName := strings.ReplaceAll(strings.ToLower(val.GetName()), _config.Global.Naming.NodeServiceSuffix, "")
						if !IsTcpNodeByEnum(nodeName) {
							continue
						}
						nodeList = append(nodeList, val.GetName())
					}
				}
			}
		}
		internal.NodeEnumCppQualifiedType = nodeEnumCppQualified

		cppData := struct {
			NodeList             []string
			NodeEnumCppQualified string
		}{
			NodeList:             nodeList,
			NodeEnumCppQualified: nodeEnumCppQualified,
		}

		if err := utils2.RenderTemplateToFile("internal/template/node_util.h.tmpl", _config.Global.Paths.GenUtilHeadFile, cppData); err != nil {
			logger.Global.Fatal("Failed to generate node util: header template rendering failed",
				zap.String("file_path", _config.Global.Paths.GenUtilHeadFile),
				zap.String("template", "internal/template/node_util.h.tmpl"),
				zap.Error(err),
			)
		}

		if err := utils2.RenderTemplateToFile("internal/template/node_util.cpp.tmpl", _config.Global.Paths.GenUtilCppFile, cppData); err != nil {
			logger.Global.Fatal("Failed to generate node util: cpp template rendering failed",
				zap.String("file_path", _config.Global.Paths.GenUtilCppFile),
				zap.String("template", "internal/template/node_util.cpp.tmpl"),
				zap.Error(err),
			)
		}
	}()
}
