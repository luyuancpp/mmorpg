package gen

import (
	"gengo/config"
	"gengo/util"
	"log"
	"os"
)

func writeEventCppHandler(fdProto os.DirEntry, dst string) {
	util.Wg.Done()

}

func WriteEventHandlerFile() {
	for i := 0; i < len(config.ProtoDirs[config.EventProtoDirIndex]); i++ {
		fds, err := os.ReadDir(config.ProtoDirs[i])
		if err != nil {
			log.Fatal(err)
			continue
		}
		for _, fd := range fds {
			if !util.IsProtoFile(fd) {
				continue
			}
			util.Wg.Add(1)
			writeEventCppHandler(fd)
		}
	}
}
