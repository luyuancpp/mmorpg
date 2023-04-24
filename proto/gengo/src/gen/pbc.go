package gen

import (
	"bytes"
	"fmt"
	"gengo/config"
	"log"
	"os"
	"os/exec"
	"path/filepath"
)

type ProtoMd5 struct {
	ProtoPath       string
	ProtoCppOutPath string
	NeedBuild       bool
}

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
		go func() {
			defer Wg.Done()
			var same bool
			var err error
			fileName := protoPath + fd.Name()
			md5FileName := protoMd5Path + fd.Name() + config.Md5Ex
			if same, err = CompareByMd5Ex(fileName, md5FileName); same {
				return
			}
			cmd := exec.Command("../../protoc",
				"--cpp_out="+config.PbcOutDir,
				fileName,
				"-I=../../",
				"-I=../../common_proto/",
				"-I=../../component_proto/",
				"-I=../../event_proto/",
				"-I=../../logic_proto/",
				"-I=../../../third_party/protobuf/src/")
			var out bytes.Buffer
			var stderr bytes.Buffer
			cmd.Stdout = &out
			cmd.Stderr = &stderr
			err = cmd.Run()
			//fmt.Println(cmd.String())
			if err != nil {
				fmt.Println(fmt.Sprint(err) + ": " + stderr.String())
				log.Fatal(err)
			}
			err = WriteToMd5ExFile(fileName, md5FileName)
			if err != nil {
				log.Fatal(err)
			}
		}()
	}
	return err
}

func Pbc() {
	for i := 0; i < len(config.ProtoDirs); i++ {
		BuildProto(config.ProtoDirs[i], config.ProtoMd5Dirs[i])
	}
}
