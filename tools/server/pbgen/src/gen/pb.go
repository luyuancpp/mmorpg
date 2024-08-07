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
	// Read directory entries
	var fds []os.DirEntry
	if fds, err = os.ReadDir(protoPath); err != nil {
		return err
	}

	// Process each protobuf file in the directory
	for _, fd := range fds {
		if !util.IsProtoFile(fd) {
			continue
		}

		// Concurrent execution for each file
		util.Wg.Add(1)
		go func(fd os.DirEntry) {
			defer util.Wg.Done()

			// Construct file paths
			fileName := protoPath + fd.Name()
			md5FileName := protoMd5Path + fd.Name() + config.Md5Ex
			dstFileName := strings.Replace(fileName, config.ProtoDir, config.PbcOutDir, 1)
			dstFileName = strings.Replace(dstFileName, config.ProtoEx, config.ProtoPbcEx, 1)

			// Check if files with same MD5 and destinations exist
			fileSame, err := util.IsSameMD5(fileName, md5FileName)
			if fileSame && util.FileExists(fileName) && util.FileExists(md5FileName) && util.FileExists(dstFileName) {
				return
			}

			// Determine the operating system type
			sysType := runtime.GOOS
			var cmd *exec.Cmd
			if sysType == `linux` {
				// Command for Linux
				cmd = exec.Command("protoc",
					"--cpp_out="+config.PbcOutDir,
					fileName,
					"-I="+config.ProtoDir,
					"-I="+config.ProtoDir+"common/",
					"-I="+config.ProtoDir+"logic/",
					"--proto_path="+config.ProjectDir+"/third_party/protobuf/src/")
			} else {
				// Command for other systems (presumably Windows)
				cmd = exec.Command("./protoc.exe",
					"--cpp_out="+config.PbcOutDir,
					fileName,
					"-I="+config.ProtoDir,
					"-I="+config.ProtoDir+"common/",
					"-I="+config.ProtoDir+"logic/",
					"--proto_path="+config.ProjectDir+"/third_party/protobuf/src/")
			}

			// Execute the command and handle errors
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

			// Write MD5 data to file
			err = util.WriteToMd5ExFile(fileName, md5FileName)
			if err != nil {
				log.Fatal(err)
			}
		}(fd)
	}
	return err
}

func BuildProtoGrpc(protoPath string, protoMd5Path string) (err error) {
	// Read directory entries
	var fds []os.DirEntry
	if fds, err = os.ReadDir(protoPath); err != nil {
		return err
	}

	// Process each protobuf file in the directory
	for _, fd := range fds {
		if !util.IsProtoFile(fd) {
			continue
		}
		// Check if the file is listed in the GrpcServiceFileMap (assumed global variable)
		if _, ok := GrpcServiceFileMap.Load(fd.Name()); !ok {
			continue
		}

		// Concurrent execution for each file
		util.Wg.Add(1)
		go func(fd os.DirEntry) {
			defer util.Wg.Done()

			// Construct file paths
			fileName := protoPath + fd.Name()
			md5FileName := protoMd5Path + fd.Name() + config.GrpcEx + config.Md5Ex
			dstFileName := strings.Replace(fileName, config.ProtoDir, config.GrpcOutDir, 1)
			dstFileName = strings.Replace(dstFileName, config.ProtoEx, config.GrpcPbcEx, 1)

			// Check if files with same MD5 and destinations exist
			fileSame, err := util.IsSameMD5(fileName, md5FileName)
			if fileSame && util.FileExists(fileName) && util.FileExists(md5FileName) && util.FileExists(dstFileName) {
				return
			}

			// Determine the operating system type
			sysType := runtime.GOOS
			var cmd *exec.Cmd
			if sysType == `linux` {
				// Command for Linux
				cmd = exec.Command("protoc",
					"--grpc_out="+config.GrpcOutDir,
					"--plugin=protoc-gen-grpc=grpc_cpp_plugin",
					fileName,
					"--proto_path="+config.ProtoDir,
					"--proto_path="+config.ProtoDir+"common/",
					"--proto_path="+config.ProtoDir+"logic/",
					"--proto_path="+config.ProjectDir+"/third_party/protobuf/src/")
			} else {
				// Command for other systems (presumably Windows)
				cmd = exec.Command("./protoc.exe",
					"--grpc_out="+config.GrpcOutDir,
					"--plugin=protoc-gen-grpc=grpc_cpp_plugin.exe",
					fileName,
					"--proto_path="+config.ProtoDir,
					"--proto_path="+config.ProtoDir+"common/",
					"--proto_path="+config.ProtoDir+"logic/",
					"--proto_path="+config.ProjectDir+"/third_party/protobuf/src/")
			}

			// Execute the command and handle errors
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

			// Write MD5 data to file
			err = util.WriteToMd5ExFile(fileName, md5FileName)
			if err != nil {
				log.Fatal(err)
			}
		}(fd)
	}
	return err
}

func BuildProtoGoLogin(protoPath string, protoMd5Path string) (err error) {
	// Read directory entries
	var fds []os.DirEntry
	if fds, err = os.ReadDir(protoPath); err != nil {
		return err
	}

	// Process each protobuf file in the directory
	for _, fd := range fds {
		if !util.IsProtoFile(fd) {
			continue
		}
		// Skip the DbProtoName and check for specific directories
		if fd.Name() == config.DbProtoName {
			continue
		}
		if !(strings.Contains(protoPath, config.ProtoDirNames[config.CommonProtoDirIndex]) ||
			strings.Contains(protoPath, config.ProtoDirNames[config.ComponentProtoDirIndex])) {
			return
		}

		// Concurrent execution for each file
		util.Wg.Add(1)
		go func(fd os.DirEntry) {
			defer util.Wg.Done()

			// Construct file paths
			fileName := protoPath + fd.Name()
			md5FileName := protoMd5Path + fd.Name() + config.LoginGoMd5Ex + config.Md5Ex
			dstFileName := config.LoginGoDir + fd.Name()
			dstFileName = strings.Replace(dstFileName, config.ProtoEx, config.ProtoGoEx, 1)

			// Check if files with same MD5 and destinations exist
			fileSame, err := util.IsSameMD5(fileName, md5FileName)
			if fileSame && util.FileExists(fileName) && util.FileExists(md5FileName) && util.FileExists(dstFileName) {
				return
			}

			// Determine the operating system type
			sysType := runtime.GOOS
			var cmd *exec.Cmd
			if sysType == `linux` {
				// Command for Linux
				cmd = exec.Command("protoc",
					"--go_out="+config.LoginDir,
					fileName,
					"--proto_path="+config.ProtoDir,
					"-I="+config.ProtoDir+"common/",
					"-I="+config.ProtoDir+"logic/",
					"--proto_path="+config.ProjectDir+"/third_party/protobuf/src/")
			} else {
				// Command for other systems (presumably Windows)
				cmd = exec.Command("./protoc.exe",
					"--go_out="+config.LoginDir,
					fileName,
					"--proto_path="+config.ProtoDir,
					"-I="+config.ProtoDir+"common/",
					"-I="+config.ProtoDir+"logic/",
					"--proto_path="+config.ProjectDir+"/third_party/protobuf/src/")
			}

			// Execute the command and handle errors
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

			// Write MD5 data to file
			err = util.WriteToMd5ExFile(fileName, md5FileName)
			if err != nil {
				log.Fatal(err)
			}
		}(fd)
	}
	return err
}

func BuildProtoGoDb(protoPath string, protoMd5Path string) (err error) {
	// Read directory entries
	var fds []os.DirEntry
	if fds, err = os.ReadDir(protoPath); err != nil {
		return err
	}

	// Process each protobuf file in the directory
	for _, fd := range fds {
		// Skip non-protobuf files
		if !util.IsProtoFile(fd) {
			continue
		}

		// Skip the file if it matches the DBProtoName configuration
		if fd.Name() == config.DbProtoName {
			continue
		}

		// Skip if the directory path does not match specific criteria
		if !(strings.Contains(protoPath, config.ProtoDirNames[config.CommonProtoDirIndex]) ||
			strings.Contains(protoPath, config.ProtoDirNames[config.ComponentProtoDirIndex])) {
			return // This might need to be handled differently
		}

		// Add a goroutine for each protobuf file processing
		util.Wg.Add(1)
		go func(fd os.DirEntry) {
			defer util.Wg.Done()

			// Construct file paths
			fileName := protoPath + fd.Name()
			md5FileName := protoMd5Path + fd.Name() + config.DBGoMd5Ex + config.Md5Ex
			dstFileName := config.DbGoDir + fd.Name()
			dstFileName = strings.Replace(dstFileName, config.ProtoEx, config.ProtoGoEx, 1)

			// Check if files with same MD5 and destinations exist
			fileSame, err := util.IsSameMD5(fileName, md5FileName)
			if fileSame && util.FileExists(fileName) && util.FileExists(md5FileName) && util.FileExists(dstFileName) {
				return
			}

			// Determine the operating system type
			sysType := runtime.GOOS
			var cmd *exec.Cmd
			if sysType == `linux` {
				// Command for Linux
				cmd = exec.Command("protoc",
					"--go_out="+config.DbGoDir,
					fileName,
					"--proto_path="+config.ProtoDir,
					"-I="+config.ProtoDir+"common/",
					"-I="+config.ProtoDir+"logic/",
					"--proto_path="+config.ProjectDir+"/third_party/protobuf/src/")
			} else {
				// Command for other systems (presumably Windows)
				cmd = exec.Command("./protoc.exe",
					"--go_out="+config.DbGoDir,
					fileName,
					"--proto_path="+config.ProtoDir,
					"-I="+config.ProtoDir+"common/",
					"-I="+config.ProtoDir+"logic/",
					"--proto_path="+config.ProjectDir+"/third_party/protobuf/src/")
			}

			// Execute the command and capture output/error
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

			// Write MD5 data to file upon successful generation
			err = util.WriteToMd5ExFile(fileName, md5FileName)
			if err != nil {
				log.Fatal(err)
			}
		}(fd)
	}

	return err
}

func BuildProtoGoClient(protoPath string, protoMd5Path string) (err error) {
	// Read directory entries
	var fds []os.DirEntry
	if fds, err = os.ReadDir(protoPath); err != nil {
		return err
	}

	// Process each protobuf file in the directory
	for _, fd := range fds {
		// Skip non-protobuf files
		if !util.IsProtoFile(fd) {
			continue
		}

		// Skip the file if it matches the DbProtoName configuration
		if fd.Name() == config.DbProtoName {
			continue
		}

		// Add a goroutine for each protobuf file processing
		util.Wg.Add(1)
		go func(fd os.DirEntry) {
			defer util.Wg.Done()

			// Construct file paths
			fileName := protoPath + fd.Name()
			md5FileName := protoMd5Path + fd.Name() + config.ClientGoMd5Ex + config.Md5Ex
			dstFileName := config.ClientGoPbDir + fd.Name()
			dstFileName = strings.Replace(dstFileName, config.ProtoEx, config.ProtoGoEx, 1)

			// Check if files with same MD5 and destinations exist
			fileSame, err := util.IsSameMD5(fileName, md5FileName)
			if fileSame && util.FileExists(fileName) && util.FileExists(md5FileName) && util.FileExists(dstFileName) {
				return
			}

			// Determine the operating system type
			sysType := runtime.GOOS
			var cmd *exec.Cmd
			if sysType == `linux` {
				// Command for Linux
				cmd = exec.Command("protoc",
					"--go_out="+config.ClientGoOutDir,
					fileName,
					"--proto_path="+config.ProtoDir,
					"-I="+config.ProtoDir+"common/",
					"-I="+config.ProtoDir+"logic/",
					"--proto_path="+config.ProjectDir+"/third_party/protobuf/src/")
			} else {
				// Command for other systems (presumably Windows)
				cmd = exec.Command("./protoc.exe",
					"--go_out="+config.ClientGoOutDir,
					fileName,
					"--proto_path="+config.ProtoDir,
					"-I="+config.ProtoDir+"common/",
					"-I="+config.ProtoDir+"logic/",
					"--proto_path="+config.ProjectDir+"/third_party/protobuf/src/")
			}

			// Execute the command and capture output/error
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

			// Write MD5 data to file upon successful generation
			err = util.WriteToMd5ExFile(fileName, md5FileName)
			if err != nil {
				log.Fatal(err)
			}
		}(fd)
	}

	return err
}

func BuildAllProtoc() {
	// Iterate over configured proto directories
	for i := 0; i < len(config.ProtoDirs); i++ {
		go func(i int) {
			// Execute functions concurrently for each directory
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
