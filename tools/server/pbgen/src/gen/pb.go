package gen

import (
	"bytes"
	"fmt"
	"log"
	"os"
	"os/exec"
	"pbgen/config"
	"pbgen/util"
	"runtime"
	"strings"
)

func BuildProto(protoPath string, protoMd5Path string) (err error) {
	var fds []os.DirEntry
	if fds, err = os.ReadDir(protoPath); err != nil {
		return err
	}
	for _, fd := range fds {
		if !util.IsProtoFile(fd) {
			continue
		}
		util.Wg.Add(1)
		go func(fd os.DirEntry) {
			defer util.Wg.Done()
			fileName := protoPath + fd.Name()
			md5FileName := protoMd5Path + fd.Name() + config.Md5Ex
			fileSame, err := IsSameMD5(fileName, md5FileName)
			dstFileName := strings.Replace(fileName, config.ProtoDir, config.PbcOutDir, 1)
			dstFileName = strings.Replace(dstFileName, config.ProtoEx, config.ProtoPbcEx, 1)
			if fileSame &&
				util.FileExists(fileName) &&
				util.FileExists(md5FileName) &&
				util.FileExists(dstFileName) {
				return
			}

			sysType := runtime.GOOS
			var cmd *exec.Cmd
			if sysType == `linux` {
				cmd = exec.Command("protoc",
					"--cpp_out="+config.PbcOutDir,
					fileName,
					"-I="+config.ProtoDir,
					"-I="+config.ProtoDir+"common_proto/",
					"-I="+config.ProtoDir+"component_proto/",
					"-I="+config.ProtoDir+"event_proto/",
					"-I="+config.ProtoDir+"logic_proto/",
					"-I="+config.ProtoDir+"constants_proto/",
					"--proto_path="+config.ProjectDir+"/third_party/protobuf/src/")

			} else {
				cmd = exec.Command("./protoc.exe",
					"--cpp_out="+config.PbcOutDir,
					fileName,
					"-I="+config.ProtoDir,
					"-I="+config.ProtoDir+"common_proto/",
					"-I="+config.ProtoDir+"component_proto/",
					"-I="+config.ProtoDir+"event_proto/",
					"-I="+config.ProtoDir+"logic_proto/",
					"-I="+config.ProtoDir+"constants_proto/",
					"--proto_path="+config.ProjectDir+"/third_party/protobuf/src/")
			}

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

func BuildProtoGrpc(protoPath string, protoMd5Path string) (err error) {
	var fds []os.DirEntry
	if fds, err = os.ReadDir(protoPath); err != nil {
		return err
	}

	for _, fd := range fds {
		if !util.IsProtoFile(fd) {
			continue
		}
		if _, ok := GrpcServiceFileMap.Load(fd.Name()); !ok {
			continue
		}
		util.Wg.Add(1)
		go func(fd os.DirEntry) {
			defer util.Wg.Done()
			fileName := protoPath + fd.Name()
			md5FileName := protoMd5Path + fd.Name() + config.GrpcEx + config.Md5Ex
			fileSame, err := IsSameMD5(fileName, md5FileName)
			dstFileName := strings.Replace(fileName, config.ProtoDir, config.GrpcOutDir, 1)
			dstFileName = strings.Replace(dstFileName, config.ProtoEx, config.GrpcPbcEx, 1)
			if fileSame &&
				util.FileExists(fileName) &&
				util.FileExists(md5FileName) &&
				util.FileExists(dstFileName) {
				return
			}

			sysType := runtime.GOOS
			var cmd *exec.Cmd
			if sysType == `linux` {
				cmd = exec.Command("protoc",
					"--grpc_out="+config.GrpcOutDir,
					"--plugin=protoc-gen-grpc=grpc_cpp_plugin",
					fileName,
					"--proto_path="+config.ProtoDir,
					"--proto_path="+config.ProtoDir+"common_proto/",
					"--proto_path="+config.ProtoDir+"component_proto/",
					"--proto_path="+config.ProtoDir+"event_proto/",
					"--proto_path="+config.ProtoDir+"logic_proto/",
					"-I="+config.ProtoDir+"constants_proto/",
					"--proto_path="+config.ProjectDir+"/third_party/protobuf/src/")
			} else {
				cmd = exec.Command("./protoc.exe",
					"--grpc_out="+config.GrpcOutDir,
					"--plugin=protoc-gen-grpc=grpc_cpp_plugin.exe",
					fileName,
					"--proto_path="+config.ProtoDir,
					"--proto_path="+config.ProtoDir+"common_proto/",
					"--proto_path="+config.ProtoDir+"component_proto/",
					"--proto_path="+config.ProtoDir+"event_proto/",
					"--proto_path="+config.ProtoDir+"logic_proto/",
					"-I="+config.ProtoDir+"constants_proto/",
					"--proto_path="+config.ProjectDir+"/third_party/protobuf/src/")
			}

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

func BuildProtoGoLogin(protoPath string, protoMd5Path string) (err error) {
	var fds []os.DirEntry
	if fds, err = os.ReadDir(protoPath); err != nil {
		return err
	}
	for _, fd := range fds {
		if !util.IsProtoFile(fd) {
			continue
		}

		if !(strings.Contains(protoPath, config.ProtoDirNames[config.CommonProtoDirIndex]) ||
			strings.Contains(protoPath, config.ProtoDirNames[config.ComponentProtoDirIndex])) {
			return
		}

		util.Wg.Add(1)
		go func(fd os.DirEntry) {
			defer util.Wg.Done()
			fileName := protoPath + fd.Name()
			md5FileName := protoMd5Path + fd.Name() + config.LoginGoMd5Ex + config.Md5Ex
			fileSame, err := IsSameMD5(fileName, md5FileName)
			dstFileName := config.LoginGoDir + fd.Name()
			dstFileName = strings.Replace(dstFileName, config.ProtoEx, config.ProtoGoEx, 1)
			if fileSame &&
				util.FileExists(fileName) &&
				util.FileExists(md5FileName) &&
				util.FileExists(dstFileName) {
				return
			}

			sysType := runtime.GOOS
			var cmd *exec.Cmd
			if sysType == `linux` {
				cmd = exec.Command("protoc",
					"--go_out="+config.LoginGoDir,
					fileName,
					"--proto_path="+config.ProtoDir,
					"-I="+config.ProtoDir+"common_proto/",
					"-I="+config.ProtoDir+"component_proto/",
					"-I="+config.ProtoDir+"event_proto/",
					"-I="+config.ProtoDir+"logic_proto/",
					"-I="+config.ProtoDir+"constants_proto/",
					"--proto_path="+config.ProjectDir+"/third_party/protobuf/src/")
			} else {
				cmd = exec.Command("./protoc.exe",
					"--go_out="+config.LoginGoDir,
					fileName,
					"--proto_path="+config.ProtoDir,
					"-I="+config.ProtoDir+"common_proto/",
					"-I="+config.ProtoDir+"component_proto/",
					"-I="+config.ProtoDir+"event_proto/",
					"-I="+config.ProtoDir+"logic_proto/",
					"-I="+config.ProtoDir+"constants_proto/",
					"--proto_path="+config.ProjectDir+"/third_party/protobuf/src/")
			}

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

func BuildProtoGoDb(protoPath string, protoMd5Path string) (err error) {
	var fds []os.DirEntry
	if fds, err = os.ReadDir(protoPath); err != nil {
		return err
	}
	for _, fd := range fds {
		if !util.IsProtoFile(fd) {
			continue
		}

		if !(strings.Contains(protoPath, config.ProtoDirNames[config.CommonProtoDirIndex]) ||
			strings.Contains(protoPath, config.ProtoDirNames[config.ComponentProtoDirIndex])) {
			return
		}

		util.Wg.Add(1)
		go func(fd os.DirEntry) {
			defer util.Wg.Done()
			fileName := protoPath + fd.Name()
			md5FileName := protoMd5Path + fd.Name() + config.DBGoMd5Ex + config.Md5Ex
			fileSame, err := IsSameMD5(fileName, md5FileName)
			dstFileName := config.DbGoDir + fd.Name()
			dstFileName = strings.Replace(dstFileName, config.ProtoEx, config.ProtoGoEx, 1)
			if fileSame &&
				util.FileExists(fileName) &&
				util.FileExists(md5FileName) &&
				util.FileExists(dstFileName) {
				return
			}

			sysType := runtime.GOOS
			var cmd *exec.Cmd
			if sysType == `linux` {
				cmd = exec.Command("protoc",
					"--go_out="+config.DbGoDir,
					fileName,
					"--proto_path="+config.ProtoDir,
					"-I="+config.ProtoDir+"common_proto/",
					"-I="+config.ProtoDir+"component_proto/",
					"-I="+config.ProtoDir+"event_proto/",
					"-I="+config.ProtoDir+"logic_proto/",
					"-I="+config.ProtoDir+"constants_proto/",
					"--proto_path="+config.ProjectDir+"/third_party/protobuf/src/")
			} else {
				cmd = exec.Command("./protoc.exe",
					"--go_out="+config.DbGoDir,
					fileName,
					"--proto_path="+config.ProtoDir,
					"-I="+config.ProtoDir+"common_proto/",
					"-I="+config.ProtoDir+"component_proto/",
					"-I="+config.ProtoDir+"event_proto/",
					"-I="+config.ProtoDir+"logic_proto/",
					"-I="+config.ProtoDir+"constants_proto/",
					"--proto_path="+config.ProjectDir+"/third_party/protobuf/src/")
			}

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

func BuildProtoGoClient(protoPath string, protoMd5Path string) (err error) {
	var fds []os.DirEntry
	if fds, err = os.ReadDir(protoPath); err != nil {
		return err
	}
	for _, fd := range fds {
		if !util.IsProtoFile(fd) {
			continue
		}

		if fd.Name() == "db_base.proto" {
			return
		}

		util.Wg.Add(1)
		go func(fd os.DirEntry) {
			defer util.Wg.Done()
			fileName := protoPath + fd.Name()
			md5FileName := protoMd5Path + fd.Name() + config.ClientGoMd5Ex + config.Md5Ex
			fileSame, err := IsSameMD5(fileName, md5FileName)
			dstFileName := config.ClientGoPbDir + fd.Name()
			dstFileName = strings.Replace(dstFileName, config.ProtoEx, config.ProtoGoEx, 1)
			if fileSame &&
				util.FileExists(fileName) &&
				util.FileExists(md5FileName) &&
				util.FileExists(dstFileName) {
				return
			}

			sysType := runtime.GOOS
			var cmd *exec.Cmd
			if sysType == `linux` {
				cmd = exec.Command("protoc",
					"--go_out="+config.ClientGoOutDir,
					fileName,
					"--proto_path="+config.ProtoDir,
					"-I="+config.ProtoDir+"common_proto/",
					"-I="+config.ProtoDir+"component_proto/",
					"-I="+config.ProtoDir+"event_proto/",
					"-I="+config.ProtoDir+"logic_proto/",
					"-I="+config.ProtoDir+"constants_proto/",
					"--proto_path="+config.ProjectDir+"/third_party/protobuf/src/")
			} else {
				cmd = exec.Command("./protoc.exe",
					"--go_out="+config.ClientGoOutDir,
					fileName,
					"--proto_path="+config.ProtoDir,
					"-I="+config.ProtoDir+"common_proto/",
					"-I="+config.ProtoDir+"component_proto/",
					"-I="+config.ProtoDir+"event_proto/",
					"-I="+config.ProtoDir+"logic_proto/",
					"-I="+config.ProtoDir+"constants_proto/",
					"--proto_path="+config.ProjectDir+"/third_party/protobuf/src/")
			}

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
		go func(i int) {
			err := BuildProto(config.ProtoDirs[i], config.ProtoMd5Dirs[i])
			if err != nil {
				log.Fatal(err)
			}
		}(i)
		go func(i int) {
			err := BuildProtoGrpc(config.ProtoDirs[i], config.ProtoMd5Dirs[i])
			if err != nil {
				log.Fatal(err)
			}
		}(i)
		go func(i int) {
			err := BuildProtoGoClient(config.ProtoDirs[i], config.ProtoMd5Dirs[i])
			if err != nil {
				log.Fatal(err)
			}
		}(i)

		go func(i int) {
			err := BuildProtoGoLogin(config.ProtoDirs[i], config.ProtoMd5Dirs[i])
			if err != nil {
				log.Fatal(err)
			}
		}(i)

		go func(i int) {
			err := BuildProtoGoDb(config.ProtoDirs[i], config.ProtoMd5Dirs[i])
			if err != nil {
				log.Fatal(err)
			}
		}(i)
	}
}
