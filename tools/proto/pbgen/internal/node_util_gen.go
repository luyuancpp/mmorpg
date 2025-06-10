package internal

import (
	"log"
	"os"
	"path"
	"pbgen/config"
	"pbgen/util"
)

func GenNodeUtil() {
	serviceInfoList := make([]*RPCServiceInfo, 0)
	for _, service := range GlobalRPCServiceList {
		if service.CcGenericServices() {
			continue
		}
		if util.IsPathInProtoDirs(service.Path(), config.DbProtoDirIndex) {
			continue
		}
		serviceInfoList = append(serviceInfoList, service)
	}

	os.MkdirAll(path.Dir(config.GrpcInitFileCppPath), os.FileMode(0777))

	cppData := struct {
		ServiceInfo []*RPCServiceInfo
	}{
		ServiceInfo: serviceInfoList,
	}

	if err := RenderTemplateToFile("internal/gen/template/node_util.h.tmpl", config.GrpcInitFileCppPath, cppData); err != nil {
		log.Fatal(err)
	}

	if err := RenderTemplateToFile("internal/gen/template/node_util.cpp.tmpl", config.GrpcInitFileHeadPath, cppData); err != nil {
		log.Fatal(err)
	}
}
