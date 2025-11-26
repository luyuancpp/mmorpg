package cpp

import (
	"log"
	"os"
	"path"
	"pbgen/config"
	"pbgen/internal"
	utils2 "pbgen/internal/utils"
	"strings"
)

func GenNodeUtil() {
	utils2.Wg.Add(1)
	go func() {
		defer utils2.Wg.Done()

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

		if err := utils2.RenderTemplateToFile("internal/template/node_util.h.tmpl", config.GenUtilFileHeadPath, cppData); err != nil {
			log.Fatal(err)
		}

		if err := utils2.RenderTemplateToFile("internal/template/node_util.cpp.tmpl", config.GenUtilFileCppPath, cppData); err != nil {
			log.Fatal(err)
		}
	}()
}
