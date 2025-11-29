package cpp

import (
	"log"
	"os"
	"path"
	"pbgen/config"
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

		os.MkdirAll(path.Dir(config.GenUtilFileHeadPath), os.FileMode(0777))

		nodeList := make([]string, 0, len(config.ProtoDirectoryNames))
		for _, file := range internal.FdSet.File {
			for _, enumDesc := range file.EnumType {
				if enumDesc.GetName() == config.NodeEnumName {
					for _, val := range enumDesc.Value {
						nodeName := strings.ReplaceAll(strings.ToLower(val.GetName()), config.NodeServiceSuffix, "")
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
