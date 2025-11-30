package cpp

import (
	"log"
	"os"
	"path"
	"pbgen/internal"
	_config "pbgen/internal/config"
	utils2 "pbgen/internal/utils"
	"strings"
	"sync"
)

func GenNodeUtil(wg *sync.WaitGroup) {
	wg.Add(1)
	go func() {
		defer wg.Done()

		err := os.MkdirAll(path.Dir(_config.Global.Paths.GenUtilHeadFile), os.FileMode(0777))
		if err != nil {
			log.Fatal(err)
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
			log.Fatal(err)
		}

		if err := utils2.RenderTemplateToFile("internal/template/node_util.cpp.tmpl", _config.Global.Paths.GenUtilCppFile, cppData); err != nil {
			log.Fatal(err)
		}
	}()
}
