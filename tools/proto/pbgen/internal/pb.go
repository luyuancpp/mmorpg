package internal

import (
	"bytes"
	"fmt"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"
	"log"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"pbgen/config"
	"pbgen/util"
	"runtime"
	"strings"
	"sync"
)

func BuildProto(protoPath string) (err error) {
	fds, err := os.ReadDir(protoPath)
	if err != nil {
		return err
	}

	os.MkdirAll(config.PbcProtoOutputDirectory, os.FileMode(0777))

	var wg sync.WaitGroup

	for _, fd := range fds {

		// Execute processing in a goroutine
		wg.Add(1)
		go func(fd os.DirEntry) {
			defer wg.Done()

			if !util.IsProtoFile(fd) {
				return
			}

			fileName := protoPath + fd.Name()

			dstFileName := strings.Replace(fileName, config.ProtoDir, config.PbcProtoOutputDirectory, 1)
			dstFileHeadName := strings.Replace(dstFileName, config.ProtoEx, config.ProtoPbhEx, 1)
			dstFileCppName := strings.Replace(dstFileName, config.ProtoEx, config.ProtoPbcEx, 1)

			protoRelativePath := strings.Replace(protoPath, config.ProjectDir, "", -1)
			newBaseDir := path.Dir(config.PbcTempDirectory + protoRelativePath)
			tempHeadFileName := filepath.Join(newBaseDir, filepath.Base(dstFileHeadName))
			tempCppFileName := filepath.Join(newBaseDir, filepath.Base(dstFileCppName))

			err := os.MkdirAll(newBaseDir, os.FileMode(0777))
			if err != nil {
				return
			}

			if err := generateCppFiles(fileName, config.PbcTempDirectory); err != nil {
				log.Fatal(err)
			}

			err = CopyFileIfChanged(dstFileCppName, tempCppFileName)
			if err != nil {
				log.Fatal(err)
				return
			}
			err = CopyFileIfChanged(dstFileHeadName, tempHeadFileName)
			if err != nil {
				log.Fatal(err)
				return
			}
		}(fd)
	}

	// Wait for all goroutines to finish
	wg.Wait()
	return nil
}

// Function to generate C++ files using protoc
func generateCppFiles(fileName, outputDir string) error {
	sysType := runtime.GOOS
	var cmd *exec.Cmd

	if sysType == "linux" {
		cmd = exec.Command("protoc",
			"--cpp_out="+outputDir,
			fileName,
			"-I="+config.ProtoParentIncludePathDir,
			"--proto_path="+config.ProtoBufferDirectory)
	} else {
		cmd = exec.Command("./protoc.exe",
			"--cpp_out="+outputDir,
			fileName,
			"-I="+config.ProtoParentIncludePathDir,
			"--proto_path="+config.ProtoBufferDirectory)
	}

	var out bytes.Buffer
	var stderr bytes.Buffer
	cmd.Stdout = &out
	cmd.Stderr = &stderr

	err := cmd.Run()
	if err != nil {
		fmt.Println(fmt.Sprint(err) + ": " + stderr.String())
		return err
	}

	return nil
}

func BuildProtoGrpc(protoPath string) (err error) {
	// Read directory entries
	var fds []os.DirEntry
	if fds, err = os.ReadDir(protoPath); err != nil {
		return err
	}

	os.MkdirAll(config.GrpcTempDirectory, os.FileMode(0777))
	os.MkdirAll(config.GrpcOutputDirectory, os.FileMode(0777))

	basePath := strings.ToLower(path.Base(protoPath))
	if _, ok := config.GrpcServices[basePath]; !ok {
		return
	}

	// Process each protobuf file in the directory
	for _, fd := range fds {

		// Concurrent execution for each file
		util.Wg.Add(1)
		go func(fd os.DirEntry) {
			defer util.Wg.Done()

			if !util.IsProtoFile(fd) {
				return
			}

			// Construct the path to the descriptor file
			descFilePath := filepath.Join(config.PbDescDirectory, fd.Name()+config.ProtoDescExtension)

			// Read the descriptor file
			data, err := os.ReadFile(descFilePath)
			if err != nil {
				return
			}

			// Unmarshal the descriptor set
			fdSet := &descriptorpb.FileDescriptorSet{}
			if err := proto.Unmarshal(data, fdSet); err != nil {
				return
			}

			files := fdSet.GetFile()
			if len(files) > 0 && files[0].Options != nil && files[0].Options.CcGenericServices != nil && *files[0].Options.CcGenericServices {
				return
			}

			// Construct file paths
			fileName := protoPath + fd.Name()

			md5FileName := strings.Replace(fileName, config.ProtoDir, config.GrpcTempDirectory+config.ProtoDirName, 1)

			dir := path.Dir(md5FileName)
			err = os.MkdirAll(dir, os.FileMode(0777))
			if err != nil {
				return
			}

			// Determine the operating system type
			sysType := runtime.GOOS
			var cmd *exec.Cmd
			if sysType == `linux` {
				// Command for Linux
				cmd = exec.Command("protoc",
					"--grpc_out="+config.GrpcTempDirectory,
					"--plugin=protoc-gen-grpc=grpc_cpp_plugin",
					fileName,
					"--proto_path="+config.ProtoParentIncludePathDir,
					"--proto_path="+config.ProtoBufferDirectory)
			} else {
				// Command for other systems (presumably Windows)
				cmd = exec.Command("./protoc.exe",
					"--grpc_out="+config.GrpcTempDirectory,
					"--plugin=protoc-gen-grpc=grpc_cpp_plugin.exe",
					fileName,
					"--proto_path="+config.ProtoParentIncludePathDir,
					"--proto_path="+config.ProtoBufferDirectory)
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

			md5FileName = strings.Replace(md5FileName, config.ProtoEx, config.GrpcPbcEx, 1)

			dstFileName := strings.Replace(fileName, config.ProtoDir, config.GrpcProtoOutputDirectory, 1)
			dstFileName = strings.Replace(dstFileName, config.ProtoEx, config.GrpcPbcEx, 1)

			err = CopyFileIfChanged(md5FileName, dstFileName)
			if err != nil {
				log.Fatal(err)
				return
			}
		}(fd)
	}
	return err
}

func BuildProtoGoLogin(protoPath string) (err error) {
	// Read directory entries
	var fds []os.DirEntry
	if fds, err = os.ReadDir(protoPath); err != nil {
		return err
	}

	// Process each protobuf file in the directory
	for _, fd := range fds {

		// Concurrent execution for each file
		util.Wg.Add(1)
		go func(fd os.DirEntry) {
			defer util.Wg.Done()

			if !util.IsProtoFile(fd) {
				return
			}
			// Skip the DbProtoName and check for specific directories
			if fd.Name() == config.DbProtoFileName {
				return
			}

			if !(util.IsPathInProtoDirs(protoPath, config.CommonProtoDirIndex) ||
				util.IsPathInProtoDirs(protoPath, config.LoginProtoDirIndex) ||
				util.IsPathInProtoDirs(protoPath, config.DbProtoDirIndex) ||
				util.IsPathInProtoDirs(protoPath, config.CenterProtoDirIndex) ||
				util.IsPathInProtoDirs(protoPath, config.LogicComponentProtoDirIndex) ||
				util.IsPathInProtoDirs(protoPath, config.ConstantsDirIndex)) {
				return
			}

			// Construct file paths
			fileName := protoPath + fd.Name()
			dstFileName := config.LoginGoGameDirectory + fd.Name()
			dstFileName = strings.Replace(dstFileName, config.ProtoEx, config.ProtoGoEx, 1)

			// Determine the operating system type
			sysType := runtime.GOOS
			var cmd *exec.Cmd
			if sysType == `linux` {
				// Command for Linux
				cmd = exec.Command("protoc",
					"--go_out="+config.LoginDirectory,
					fileName,
					"--proto_path="+config.ProtoParentIncludePathDir,
					"--proto_path="+config.ProtoBufferDirectory)
			} else {
				// Command for other systems (presumably Windows)
				cmd = exec.Command("./protoc.exe",
					"--go_out="+config.LoginDirectory,
					fileName,
					"--proto_path="+config.ProtoParentIncludePathDir,
					"--proto_path="+config.ProtoBufferDirectory)
			}

			// Execute the command and handle errors
			var out bytes.Buffer
			var stderr bytes.Buffer
			cmd.Stdout = &out
			cmd.Stderr = &stderr
			err = cmd.Run()
			if err != nil {
				fmt.Println(fmt.Sprint(err) + ": " + stderr.String())
				log.Fatal(err)
			}

		}(fd)
	}
	return err
}

func BuildProtoGoDb(protoPath string) (err error) {
	// Read directory entries
	var fds []os.DirEntry
	if fds, err = os.ReadDir(protoPath); err != nil {
		return err
	}

	// Process each protobuf file in the directory
	for _, fd := range fds {
		util.Wg.Add(1)
		go func(fd os.DirEntry) {
			defer util.Wg.Done()

			if !util.IsProtoFile(fd) {
				return
			}

			// Skip the file if it matches the DBProtoName configuration
			if fd.Name() == config.DbProtoFileName {
				return
			}

			// Skip if the directory path does not match specific criteria
			if !(util.IsPathInProtoDirs(protoPath, config.CommonProtoDirIndex) ||
				util.IsPathInProtoDirs(protoPath, config.DbProtoDirIndex) ||
				util.IsPathInProtoDirs(protoPath, config.LogicComponentProtoDirIndex) ||
				util.IsPathInProtoDirs(protoPath, config.ConstantsDirIndex)) {
				return
			}

			fileName := protoPath + fd.Name()

			// Determine the operating system type
			sysType := runtime.GOOS
			var cmd *exec.Cmd
			if sysType == `linux` {
				// Command for Linux
				cmd = exec.Command("protoc",
					"--go_out="+config.DbGoDirectory,
					fileName,
					"--proto_path="+config.ProtoParentIncludePathDir,
					"--proto_path="+config.ProtoBufferDirectory)
			} else {
				// Command for other systems (presumably Windows)
				cmd = exec.Command("./protoc.exe",
					"--go_out="+config.DbGoDirectory,
					fileName,
					"--proto_path="+config.ProtoParentIncludePathDir,
					"--proto_path="+config.ProtoBufferDirectory)
			}

			var out bytes.Buffer
			var stderr bytes.Buffer
			cmd.Stdout = &out
			cmd.Stderr = &stderr
			err = cmd.Run()

			if err != nil {
				fmt.Println(fmt.Sprint(err) + ": " + stderr.String())
				log.Fatal(err)
			}

		}(fd)
	}

	return err
}

func BuildProtoDesc(protoPath string) (err error) {
	// Read directory entries

	var fds []os.DirEntry
	if fds, err = os.ReadDir(protoPath); err != nil {
		return err
	}

	os.MkdirAll(config.PbDescDirectory, os.FileMode(0777))

	// Process each protobuf file in the directory
	for _, fd := range fds {
		// Add a goroutine for each protobuf file processing
		util.Wg.Add(1)
		go func(fd os.DirEntry) {
			defer util.Wg.Done()

			if !util.IsProtoFile(fd) {
				return
			}

			// Construct file paths
			fileName := protoPath + fd.Name()

			// Determine the operating system type
			sysType := runtime.GOOS
			var cmd *exec.Cmd

			if sysType == `linux` {
				// Command for Linux
				cmd = exec.Command("protoc",
					"--descriptor_set_out="+config.PbDescDirectory+fd.Name()+config.ProtoDescExtension,
					fileName,
					"--proto_path="+config.ProtoParentIncludePathDir,
					"--proto_path="+config.ProtoBufferDirectory)
			} else {
				// Command for other systems (presumably Windows)
				cmd = exec.Command("./protoc.exe",
					"--descriptor_set_out="+config.PbDescDirectory+fd.Name()+config.ProtoDescExtension,
					fileName,
					"--proto_path="+config.ProtoParentIncludePathDir,
					"--proto_path="+config.ProtoBufferDirectory)
			}

			// Execute the command and capture output/error
			var out bytes.Buffer
			var stderr bytes.Buffer
			cmd.Stdout = &out
			cmd.Stderr = &stderr
			err = cmd.Run()
			if err != nil {
				fmt.Println(fmt.Sprint(err) + ": " + stderr.String())
				log.Fatal(err)
			}

		}(fd)
	}

	return err
}

func BuildProtoRobotGo(protoPath string) (err error) {
	// Read directory entries
	var fds []os.DirEntry
	if fds, err = os.ReadDir(protoPath); err != nil {
		return err
	}

	// Process each protobuf file in the directory
	for _, fd := range fds {

		// Add a goroutine for each protobuf file processing
		util.Wg.Add(1)
		go func(fd os.DirEntry) {
			defer util.Wg.Done()

			// Skip non-protobuf files
			if !util.IsProtoFile(fd) {
				return
			}

			// Skip the file if it matches the DbProtoName configuration
			if fd.Name() == config.DbProtoFileName {
				return
			}

			// Construct file paths
			fileName := protoPath + fd.Name()

			// Determine the operating system type
			sysType := runtime.GOOS
			var cmd *exec.Cmd
			if sysType == `linux` {
				// Command for Linux
				cmd = exec.Command("protoc",
					"--go_out="+config.RobotGoOutputDirectory,
					fileName,
					"--proto_path="+config.ProtoParentIncludePathDir,
					"--proto_path="+config.ProtoBufferDirectory)
			} else {
				// Command for other systems (presumably Windows)
				cmd = exec.Command("./protoc.exe",
					"--go_out="+config.RobotGoOutputDirectory,
					fileName,
					"--proto_path="+config.ProtoParentIncludePathDir,
					"--proto_path="+config.ProtoBufferDirectory)
			}

			// Execute the command and capture output/error
			var out bytes.Buffer
			var stderr bytes.Buffer
			cmd.Stdout = &out
			cmd.Stderr = &stderr
			err = cmd.Run()
			if err != nil {
				fmt.Println(fmt.Sprint(err) + ": " + stderr.String())
				log.Fatal(err)
			}

		}(fd)
	}

	return err
}

func BuildProtoGoDeploy(protoPath string) (err error) {
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
		if fd.Name() == config.DbProtoFileName ||
			fd.Name() == config.GameMysqlDBProtoFileName ||
			fd.Name() == config.LoginServiceProtoFileName {
			continue
		}

		if !(strings.Contains(protoPath, config.ProtoDirectoryNames[config.CommonProtoDirIndex])) {
			return
		}

		// Concurrent execution for each file
		util.Wg.Add(1)
		go func(fd os.DirEntry) {
			defer util.Wg.Done()

			// Construct file paths
			fileName := protoPath + fd.Name()

			// Determine the operating system type
			sysType := runtime.GOOS
			var cmd *exec.Cmd
			if sysType == `linux` {
				// Command for Linux
				cmd = exec.Command("protoc",
					"--go_out="+config.DeployDirectory,
					fileName,
					"--proto_path="+config.ProtoParentIncludePathDir,
					"--proto_path="+config.ProtoBufferDirectory)
			} else {
				// Command for other systems (presumably Windows)
				cmd = exec.Command("./protoc.exe",
					"--go_out="+config.DeployDirectory,
					fileName,
					"--proto_path="+config.ProtoParentIncludePathDir,
					"--proto_path="+config.ProtoBufferDirectory)
			}

			// Execute the command and handle errors
			var out bytes.Buffer
			var stderr bytes.Buffer
			cmd.Stdout = &out
			cmd.Stderr = &stderr
			err = cmd.Run()
			if err != nil {
				fmt.Println(fmt.Sprint(err) + ": " + stderr.String())
				log.Fatal(err)
			}

		}(fd)
	}
	return err
}

func BuildProtocDesc() {
	for i := 0; i < len(config.ProtoDirs); i++ {
		go func(i int) {
			err := BuildProtoDesc(config.ProtoDirs[i])
			if err != nil {
				log.Fatal(err)
			}
		}(i)
	}
}

func BuildAllProtoc() {
	// Iterate over configured proto directories
	for i := 0; i < len(config.ProtoDirs); i++ {

		go func(i int) {
			// Execute functions concurrently for each directory
			err := BuildProto(config.ProtoDirs[i])
			if err != nil {
				log.Fatal(err)
			}
		}(i)

		go func(i int) {
			err := BuildProtoGrpc(config.ProtoDirs[i])
			if err != nil {
				log.Fatal(err)
			}
		}(i)

		go func(i int) {
			err := BuildProtoRobotGo(config.ProtoDirs[i])
			if err != nil {
				log.Fatal(err)
			}
		}(i)

		go func(i int) {
			err := BuildProtoGoLogin(config.ProtoDirs[i])
			if err != nil {
				log.Fatal(err)
			}
		}(i)

		go func(i int) {
			err := BuildProtoGoDb(config.ProtoDirs[i])
			if err != nil {
				log.Fatal(err)
			}
		}(i)

		go func(i int) {
			err := BuildProtoGoDeploy(config.ProtoDirs[i])
			if err != nil {
				log.Fatal(err)
			}
		}(i)

	}
}
