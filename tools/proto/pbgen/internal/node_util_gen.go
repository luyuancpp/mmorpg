package internal

import (
	"log"
	"os"
	"path"
	"pbgen/config"
	"pbgen/util"
)

func GenNodeUtil() {
	util.Wg.Add(1)
	go func() {
		defer util.Wg.Done()

		os.MkdirAll(path.Dir(config.GenUtilFileHeadPath), os.FileMode(0777))

		nodeList := make([]string, 0, len(config.ProtoDirectoryNames))
		for _, file := range FdSet.File {
			for _, enumDesc := range file.EnumType {
				if enumDesc.GetName() == "eNodeType" {
					// 找到了 eNodeType 枚举，遍历它的所有值
					for _, val := range enumDesc.Value {
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

		if err := RenderTemplateToFile("internal/gen/template/node_util.h.tmpl", config.GenUtilFileHeadPath, cppData); err != nil {
			log.Fatal(err)
		}

		if err := RenderTemplateToFile("internal/gen/template/node_util.cpp.tmpl", config.GenUtilFileCppPath, cppData); err != nil {
			log.Fatal(err)
		}
	}()

}
