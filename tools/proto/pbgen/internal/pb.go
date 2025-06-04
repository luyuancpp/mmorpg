package internal

import (
	"bytes"
	"fmt"
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
	// Read directory entries
	fds, err := os.ReadDir(protoPath)
	if err != nil {
		return err
	}

	os.MkdirAll(config.PbcProtoOutputDirectory, os.FileMode(0777))

	// Wait group for synchronization
	var wg sync.WaitGroup

	// Process each protobuf file in the directory
	for _, fd := range fds {
		if !util.IsProtoFile(fd) {
			continue
		}

		// Execute processing in a goroutine
		wg.Add(1)
		go func(fd os.DirEntry) {
			defer wg.Done()

			// Construct file paths
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

			fileHeadSame, _ := util.IsSameMD5(dstFileHeadName, tempHeadFileName)
			fileCppSame, _ := util.IsSameMD5(dstFileCppName, tempCppFileName)

			if fileCppSame && fileHeadSame {
				return
			}

			// Generate C++ files

			util.Copy(dstFileCppName, tempCppFileName)
			util.Copy(dstFileHeadName, tempHeadFileName)
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

			// Determine the operating system type
			sysType := runtime.GOOS
			var cmd *exec.Cmd
			if sysType == `linux` {
				// Command for Linux
				cmd = exec.Command("protoc",
					"--grpc_out="+config.GrpcOutputDirectory,
					"--plugin=protoc-gen-grpc=grpc_cpp_plugin",
					fileName,
					"--proto_path="+config.ProtoParentIncludePathDir,
					"--proto_path="+config.ProtoBufferDirectory)
			} else {
				// Command for other systems (presumably Windows)
				cmd = exec.Command("./protoc.exe",
					"--grpc_out="+config.GrpcOutputDirectory,
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

			md5FileName := strings.Replace(fileName, config.ProtoDir, config.GrpcTempDirectory, 1)

			dir := path.Dir(md5FileName)
			err := os.MkdirAll(dir, os.FileMode(0777))
			if err != nil {
				return
			}

			md5FileName = strings.Replace(md5FileName, config.ProtoEx, config.GrpcPbcEx, 1)

			dstFileName := strings.Replace(fileName, config.ProtoDir, config.GrpcProtoOutputDirectory, 1)
			dstFileName = strings.Replace(dstFileName, config.ProtoEx, config.GrpcPbcEx, 1)

			// Check if files with same MD5 and destinations exist
			fileSame, err := util.IsSameMD5(dstFileName, md5FileName)
			if fileSame {
				return
			}

			_, err = util.Copy(md5FileName, dstFileName)
			if err != nil {
				fmt.Println(fmt.Sprint(err) + ": " + stderr.String())
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
		if !util.IsProtoFile(fd) {
			continue
		}
		// Skip the DbProtoName and check for specific directories
		if fd.Name() == config.DbProtoFileName {
			continue
		}

		if !(util.IsPathInProtoDirs(protoPath, config.LogicComponentProtoDirIndex) ||
			util.IsPathInProtoDirs(protoPath, config.LoginProtoDirIndex) ||
			util.IsPathInProtoDirs(protoPath, config.ConstantsDirIndex)) {
			return
		}

		// Concurrent execution for each file
		util.Wg.Add(1)
		go func(fd os.DirEntry) {
			defer util.Wg.Done()

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
		// Skip non-protobuf files
		if !util.IsProtoFile(fd) {
			continue
		}

		// Skip the file if it matches the DBProtoName configuration
		if fd.Name() == config.DbProtoFileName {
			continue
		}

		// Skip if the directory path does not match specific criteria
		if !(util.IsPathInProtoDirs(protoPath, config.CommonProtoDirIndex) ||
			util.IsPathInProtoDirs(protoPath, config.DbProtoDirIndex) ||
			util.IsPathInProtoDirs(protoPath, config.LogicComponentProtoDirIndex) ||
			util.IsPathInProtoDirs(protoPath, config.ConstantsDirIndex)) {
			return
		}

		// Add a goroutine for each protobuf file processing
		util.Wg.Add(1)
		go func(fd os.DirEntry) {
			defer util.Wg.Done()

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
		// Skip non-protobuf files
		if !util.IsProtoFile(fd) {
			continue
		}

		// Add a goroutine for each protobuf file processing
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

func BuildProtoGoClient(protoPath string) (err error) {
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
		if fd.Name() == config.DbProtoFileName {
			continue
		}

		// Add a goroutine for each protobuf file processing
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
			err := BuildProtoGoClient(config.ProtoDirs[i])
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
