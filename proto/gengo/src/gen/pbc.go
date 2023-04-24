package gen

import (
	"bytes"
	"fmt"
	"gengo/config"
	"gengo/util"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
)

func BuildProto(protoPath string, protoMd5Path string) (err error) {
	var fds []os.DirEntry
	if fds, err = os.ReadDir(protoPath); err != nil {
		return err
	}
	for _, fd := range fds {
		if fd.IsDir() || filepath.Ext(fd.Name()) != ".proto" {
			continue
		}
		Wg.Add(1)
		go func(fd os.DirEntry) {
			defer Wg.Done()
			fileName := protoPath + fd.Name()
			md5FileName := protoMd5Path + fd.Name() + config.Md5Ex
			fileSame, err := CompareByMd5Ex(fileName, md5FileName)
			dstFileName := strings.Replace(fileName, config.ProtoDir, config.PbcOutDir, 1)
			dstFileName = strings.Replace(dstFileName, config.ProtoEx, config.ProtoPbcEx, 1)
			if fileSame &&
				util.FileExists(fileName) &&
				util.FileExists(md5FileName) &&
				util.FileExists(dstFileName) {
				return
			}
			cmd := exec.Command("../../protoc",
				"--cpp_out="+config.PbcOutDir,
				fileName,
				"-I="+config.ProtoDir,
				"-I="+config.ProtoDir+"common_proto/",
				"-I="+config.ProtoDir+"component_proto/",
				"-I="+config.ProtoDir+"event_proto/",
				"-I="+config.ProtoDir+"logic_proto/",
				"-I=../../../third_party/protobuf/src/")
			var out bytes.Buffer
			var stderr bytes.Buffer
			cmd.Stdout = &out
			cmd.Stderr = &stderr
			err = cmd.Run()
			fmt.Println(cmd.String())
			if err != nil {
				fmt.Println(fmt.Sprint(err) + ": " + stderr.String())
				log.Fatal(err)
			}
			err = WriteToMd5ExFile(fileName, md5FileName)
			if err != nil {
				log.Fatal(err)
			}
		}(fd)
	}
	return err
}

func BuildAllProtoc() {
	for i := 0; i < len(config.ProtoDirs); i++ {
		BuildProto(config.ProtoDirs[i], config.ProtoMd5Dirs[i])
	}
}
