package gen

import (
	"bytes"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
)

func GenPbc(protoPath string, protoCppOutPath string, ch chan error) {
	var err error
	var fds []os.DirEntry
	if fds, err = os.ReadDir(protoPath); err != nil {
		ch <- err
	}
	for _, fd := range fds {
		if fd.IsDir() || filepath.Ext(fd.Name()) != "proto" {
			continue
		}
		cmd := exec.Command("./protoc",
			"--cpp_out="+protoCppOutPath,
			protoPath+fd.Name(),
			"-I=../",
			"-I=../common_proto/",
			"-I=../component_proto/",
			"-I=../event_proto/",
			"-I=../logic_proto/",
			"-I=../../third_party/protobuf/src/")
		var out bytes.Buffer
		var stderr bytes.Buffer
		cmd.Stdout = &out
		cmd.Stderr = &stderr
		err := cmd.Run()
		if err != nil {
			fmt.Println(cmd.String())
			fmt.Println(fmt.Sprint(err) + ": " + stderr.String())
		}
	}
	ch <- nil
}
