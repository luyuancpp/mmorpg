package internal

import (
	"bytes"
	"fmt"
	"log"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"pbgen/internal/config"
	"pbgen/util"
	"runtime"
	"strings"
)

func BuildProto(protoPath string) error {
	// 读取 proto 文件夹内容
	fds, err := os.ReadDir(protoPath)
	if err != nil {
		return err
	}

	os.MkdirAll(config.PbcProtoOutputDirectory, os.FileMode(0777))

	var protoFiles []string
	for _, fd := range fds {
		if util.IsProtoFile(fd) {
			fullPath := filepath.ToSlash(filepath.Join(protoPath, fd.Name()))
			protoFiles = append(protoFiles, fullPath)
		}
	}

	if len(protoFiles) == 0 {
		log.Println("No .proto files found in:", protoPath)
		return nil
	}

	// 调用 protoc 执行批量生成
	if err := generateCppFiles(protoFiles, config.PbcTempDirectory); err != nil {
		return err
	}

	// 复制生成的 .pb.h 和 .pb.cc 文件到目标目录（若有变化）
	for _, protoFile := range protoFiles {
		dstFileName := strings.Replace(protoFile, config.ProtoDir, config.PbcProtoOutputDirectory, 1)
		dstFileHeadName := strings.Replace(dstFileName, config.ProtoEx, config.ProtoPbhEx, 1)
		dstFileCppName := strings.Replace(dstFileName, config.ProtoEx, config.ProtoPbcEx, 1)

		protoRelativePath := strings.Replace(protoPath, config.OutputRoot, "", 1)

		tempBaseDir := filepath.ToSlash(path.Dir(config.PbcTempDirectory + protoRelativePath))
		newBaseDir := filepath.ToSlash(path.Dir(dstFileCppName))

		tempHeadFileName := filepath.Join(tempBaseDir, filepath.Base(dstFileHeadName))
		tempCppFileName := filepath.Join(tempBaseDir, filepath.Base(dstFileCppName))

		if err := os.MkdirAll(tempBaseDir, os.FileMode(0777)); err != nil {
			log.Println("mkdir failed:", err)
			continue
		}
		if err := os.MkdirAll(newBaseDir, os.FileMode(0777)); err != nil {
			log.Println("mkdir failed:", err)
			continue
		}
		if err := CopyFileIfChanged(tempCppFileName, dstFileCppName); err != nil {
			log.Println("copy .cc failed:", err)
			continue
		}
		if err := CopyFileIfChanged(tempHeadFileName, dstFileHeadName); err != nil {
			log.Println("copy .h failed:", err)
			continue
		}
	}

	return nil
}

// Function to generate C++ files using protoc
func generateCppFiles(protoFiles []string, outputDir string) error {
	sysType := runtime.GOOS
	var cmd *exec.Cmd

	args := []string{
		"--cpp_out=" + outputDir,
	}
	args = append(args, protoFiles...) // 多个 .proto 文件一起处理
	args = append(args,
		"-I="+config.ProtoParentIncludePathDir,
		"--proto_path="+config.ProtoBufferDirectory,
	)

	if sysType == "linux" {
		cmd = exec.Command("protoc", args...)
	} else {
		cmd = exec.Command("./protoc.exe", args...)
	}

	var out bytes.Buffer
	var stderr bytes.Buffer
	cmd.Stdout = &out
	cmd.Stderr = &stderr

	log.Println("Running:", cmd.String())
	if err := cmd.Run(); err != nil {
		fmt.Println("protoc error:", stderr.String())
		return err
	}

	return nil
}

func BuildProtoGrpc(protoPath string) error {
	// 读取 proto 目录文件
	fds, err := os.ReadDir(protoPath)
	if err != nil {
		return err
	}

	os.MkdirAll(config.GrpcTempDirectory, os.FileMode(0777))
	os.MkdirAll(config.GrpcOutputDirectory, os.FileMode(0777))

	basePath := strings.ToLower(path.Base(protoPath))
	if _, ok := config.GrpcServices[basePath]; !ok {
		return nil
	}

	var protoFiles []string
	for _, fd := range fds {
		if util.IsProtoFile(fd) {
			protoFiles = append(protoFiles, filepath.Join(protoPath, fd.Name()))
		}
	}

	if len(protoFiles) == 0 {
		log.Println("No .proto files found in", protoPath)
		return nil
	}

	// 构造 protoc 命令参数
	args := []string{
		"--grpc_out=" + config.GrpcTempDirectory,
	}
	if runtime.GOOS == "linux" {
		args = append(args, "--plugin=protoc-gen-grpc=grpc_cpp_plugin")
	} else {
		args = append(args, "--plugin=protoc-gen-grpc=grpc_cpp_plugin.exe")
	}
	args = append(args, protoFiles...)
	args = append(args,
		"--proto_path="+config.ProtoParentIncludePathDir,
		"--proto_path="+config.ProtoBufferDirectory,
	)

	// 构造最终命令
	var cmd *exec.Cmd
	if runtime.GOOS == "linux" {
		cmd = exec.Command("protoc", args...)
	} else {
		cmd = exec.Command("./protoc.exe", args...)
	}

	// 执行命令
	var out, stderr bytes.Buffer
	cmd.Stdout = &out
	cmd.Stderr = &stderr
	log.Println("Running command:", cmd.String())
	if err := cmd.Run(); err != nil {
		log.Println("protoc error:", stderr.String())
		return fmt.Errorf("failed to run protoc: %w", err)
	}

	// 拷贝生成文件（按文件列表）
	for _, protoFile := range protoFiles {
		protoFile = filepath.ToSlash(protoFile)

		// 源 .proto 替换为 .pb.cc/.pb.h（你的扩展名设定）
		tempCpp := strings.Replace(protoFile, config.ProtoDir, config.GrpcTempDirectory+config.ProtoDirName, 1)
		tempCpp = strings.Replace(tempCpp, config.ProtoEx, config.GrpcPbcEx, 1)
		tempHead := strings.Replace(tempCpp, config.GrpcPbcEx, config.GrpcPbhEx, 1)

		dstCpp := strings.Replace(protoFile, config.ProtoDir, config.GrpcProtoOutputDirectory, 1)
		dstCpp = strings.Replace(dstCpp, config.ProtoEx, config.GrpcPbcEx, 1)
		dstHead := strings.Replace(dstCpp, config.GrpcPbcEx, config.GrpcPbhEx, 1)

		// 创建目录
		dir := path.Dir(tempCpp)
		if err := os.MkdirAll(dir, os.FileMode(0777)); err != nil {
			log.Fatal(err)
			return err
		}

		// 拷贝文件（如内容有变）
		if err := CopyFileIfChanged(tempCpp, dstCpp); err != nil {
			log.Fatal("Failed to copy:", err)
		}

		if err := CopyFileIfChanged(tempHead, dstHead); err != nil {
			log.Fatal("Failed to copy:", err)
		}
	}

	return nil
}

func generateLoginGoProto(protoFiles []string, outputDir string) error {
	sysType := runtime.GOOS
	var cmd *exec.Cmd

	args := []string{
		"--go_out=" + outputDir,
	}
	args = append(args, protoFiles...)
	args = append(args,
		"--proto_path="+config.ProtoParentIncludePathDir,
		"--proto_path="+config.ProtoBufferDirectory,
	)

	if sysType == "linux" {
		cmd = exec.Command("protoc", args...)
	} else {
		cmd = exec.Command("./protoc.exe", args...)
	}

	var out, stderr bytes.Buffer
	cmd.Stdout = &out
	cmd.Stderr = &stderr

	log.Println("Running:", cmd.String())
	if err := cmd.Run(); err != nil {
		fmt.Println("protoc error:", stderr.String())
		return err
	}

	return nil
}

// GenerateGoGRPCFromProto processes .proto files in the given directory
// and generates Go gRPC code for allowed services.
func GenerateGoGRPCFromProto(protoPath string) error {
	// 1. 读取 protoPath 目录下的所有文件
	files, err := os.ReadDir(protoPath)
	if err != nil {
		return err
	}

	var protoFiles []string

	// 2. 筛选有效的 .proto 文件
	for _, file := range files {
		if !util.IsProtoFile(file) || file.Name() == config.DbProtoFileName {
			continue
		}

		if !isInAllowedProtoDir(protoPath) {
			continue
		}

		fullPath := filepath.ToSlash(filepath.Join(protoPath, file.Name()))
		protoFiles = append(protoFiles, fullPath)
	}

	// 3. 如果没有符合条件的 proto 文件，记录日志并退出
	if len(protoFiles) == 0 {
		log.Println("No proto files to process for login:", protoPath)
		return nil
	}

	// 4. 为所有注册的 grpc 节点目录生成 Go gRPC 代码
	for nodeDir := range config.GrpcServices {
		outputDir := filepath.Join(config.NodeGoDirectory, nodeDir)
		generateLoginGoProto(protoFiles, outputDir)
	}

	return nil
}

// isInAllowedProtoDir 判断 protoPath 是否是允许处理的目录或 grpc service 目录
func isInAllowedProtoDir(protoPath string) bool {
	baseDirName := strings.ToLower(filepath.Base(protoPath)) // 提取最后一级目录名作为 key

	if util.IsPathInProtoDirs(protoPath, config.DbProtoDirIndex) ||
		util.IsPathInProtoDirs(protoPath, config.LoginProtoDirIndex) ||
		util.IsPathInProtoDirs(protoPath, config.EtcdProtoDirIndex) {
		return false
	}
	return util.IsPathInProtoDirs(protoPath, config.CommonProtoDirIndex) ||
		util.IsPathInProtoDirs(protoPath, config.LogicComponentProtoDirIndex) ||
		util.IsPathInProtoDirs(protoPath, config.ConstantsDirIndex) ||
		config.GrpcServices[baseDirName]
}

func BuildProtoGoLogin(protoPath string) error {
	// 读取 proto 目录
	fds, err := os.ReadDir(protoPath)
	if err != nil {
		return err
	}

	var protoFiles []string
	for _, fd := range fds {
		if !util.IsProtoFile(fd) {
			continue
		}
		if fd.Name() == config.DbProtoFileName {
			continue
		}

		if !(util.IsPathInProtoDirs(protoPath, config.CommonProtoDirIndex) ||
			util.IsPathInProtoDirs(protoPath, config.LoginProtoDirIndex) ||
			util.IsPathInProtoDirs(protoPath, config.DbProtoDirIndex) ||
			util.IsPathInProtoDirs(protoPath, config.CenterProtoDirIndex) ||
			util.IsPathInProtoDirs(protoPath, config.LogicComponentProtoDirIndex) ||
			util.IsPathInProtoDirs(protoPath, config.ConstantsDirIndex)) {
			continue
		}

		fullPath := filepath.ToSlash(filepath.Join(protoPath, fd.Name()))
		protoFiles = append(protoFiles, fullPath)
	}

	if len(protoFiles) == 0 {
		log.Println("No proto files to process for login:", protoPath)
		return nil
	}

	return generateLoginGoProto(protoFiles, config.LoginDirectory)
}

func generateGoDbProto(protoFiles []string, outputDir string) error {
	sysType := runtime.GOOS
	var cmd *exec.Cmd

	args := []string{
		"--go_out=" + outputDir,
	}
	args = append(args, protoFiles...)
	args = append(args,
		"--proto_path="+config.ProtoParentIncludePathDir,
		"--proto_path="+config.ProtoBufferDirectory,
	)

	if sysType == "linux" {
		cmd = exec.Command("protoc", args...)
	} else {
		cmd = exec.Command("./protoc.exe", args...)
	}

	var out, stderr bytes.Buffer
	cmd.Stdout = &out
	cmd.Stderr = &stderr

	log.Println("Running:", cmd.String())
	if err := cmd.Run(); err != nil {
		fmt.Println("protoc error:", stderr.String())
		return err
	}

	return nil
}

func BuildProtoGoDb(protoPath string) error {
	// 读取 protoPath 下所有文件
	fds, err := os.ReadDir(protoPath)
	if err != nil {
		return err
	}

	var protoFiles []string

	for _, fd := range fds {
		if !util.IsProtoFile(fd) {
			continue
		}
		if fd.Name() == config.DbProtoFileName {
			continue
		}
		if !(util.IsPathInProtoDirs(protoPath, config.CommonProtoDirIndex) ||
			util.IsPathInProtoDirs(protoPath, config.DbProtoDirIndex) ||
			util.IsPathInProtoDirs(protoPath, config.LogicComponentProtoDirIndex) ||
			util.IsPathInProtoDirs(protoPath, config.ConstantsDirIndex)) {
			continue
		}

		fullPath := filepath.ToSlash(filepath.Join(protoPath, fd.Name()))
		protoFiles = append(protoFiles, fullPath)
	}

	if len(protoFiles) == 0 {
		log.Println("No proto files matched for Go DB generation in:", protoPath)
		return nil
	}

	return generateGoDbProto(protoFiles, config.DbGoDirectory)
}

func generateRobotGoProto(protoFiles []string, outputDir string) error {
	sysType := runtime.GOOS
	var cmd *exec.Cmd

	args := []string{
		"--go_out=" + outputDir,
	}
	args = append(args, protoFiles...)
	args = append(args,
		"--proto_path="+config.ProtoParentIncludePathDir,
		"--proto_path="+config.ProtoBufferDirectory,
	)

	if sysType == "linux" {
		cmd = exec.Command("protoc", args...)
	} else {
		cmd = exec.Command("./protoc.exe", args...)
	}

	var out, stderr bytes.Buffer
	cmd.Stdout = &out
	cmd.Stderr = &stderr

	log.Println("Running:", cmd.String())
	if err := cmd.Run(); err != nil {
		fmt.Println("protoc error:", stderr.String())
		return err
	}

	return nil
}

func BuildProtoRobotGo(protoPath string) error {
	// 读取目录下所有文件
	fds, err := os.ReadDir(protoPath)
	if err != nil {
		return err
	}

	var protoFiles []string
	for _, fd := range fds {
		if !util.IsProtoFile(fd) {
			continue
		}
		if fd.Name() == config.DbProtoFileName {
			continue
		}

		fullPath := filepath.ToSlash(filepath.Join(protoPath, fd.Name()))
		protoFiles = append(protoFiles, fullPath)
	}

	if len(protoFiles) == 0 {
		log.Println("No proto files found for robot go generation in:", protoPath)
		return nil
	}

	return generateRobotGoProto(protoFiles, config.RobotGoOutputDirectory)
}

func BuildProtocDescAllInOne() {
	util.Wg.Add(1)

	go func() {
		defer util.Wg.Done()

		var allProtoFiles []string

		for _, dir := range config.ProtoDirs {
			fds, err := os.ReadDir(dir)
			if err != nil {
			}
			for _, fd := range fds {
				if util.IsProtoFile(fd) {
					protoFile := filepath.ToSlash(filepath.Join(dir, fd.Name()))
					allProtoFiles = append(allProtoFiles, protoFile)
				}
			}
		}

		if len(allProtoFiles) == 0 {
			log.Println("No proto files found in any directory")
			return
		}

		descOut := filepath.ToSlash(config.AllInOneProtoDescFile)
		args := append([]string{
			"--descriptor_set_out=" + descOut,
			"--include_imports",
		}, allProtoFiles...)
		args = append(args,
			"--proto_path="+config.ProtoParentIncludePathDir,
			"--proto_path="+config.ProtoBufferDirectory,
		)

		var cmd *exec.Cmd
		if runtime.GOOS == "linux" {
			cmd = exec.Command("protoc", args...)
		} else {
			cmd = exec.Command("./protoc.exe", args...)
		}

		var out, stderr bytes.Buffer
		cmd.Stdout = &out
		cmd.Stderr = &stderr

		log.Println("Running:", cmd.String())
		if err := cmd.Run(); err != nil {
			log.Fatal("protoc error:", stderr.String())
		}

		log.Println("Descriptor file generated at:", descOut)
	}()
}

func BuildAllProtoc() {
	// Iterate over configured proto directories
	for i := 0; i < len(config.ProtoDirs); i++ {

		util.Wg.Add(1)
		go func(i int) {
			defer util.Wg.Done()
			// Execute functions concurrently for each directory
			err := BuildProto(config.ProtoDirs[i])
			if err != nil {
				log.Println(err)
			}
		}(i)

		util.Wg.Add(1)
		go func(i int) {
			defer util.Wg.Done()
			err := BuildProtoGrpc(config.ProtoDirs[i])
			if err != nil {
				log.Println(err)
			}
		}(i)

		util.Wg.Add(1)
		go func(i int) {
			defer util.Wg.Done()
			err := BuildProtoRobotGo(config.ProtoDirs[i])
			if err != nil {
				log.Println(err)
			}
		}(i)

		util.Wg.Add(1)
		go func(i int) {
			defer util.Wg.Done()
			err := GenerateGoGRPCFromProto(config.ProtoDirs[i])
			if err != nil {
				log.Println(err)
			}
		}(i)

		util.Wg.Add(1)
		go func(i int) {
			defer util.Wg.Done()
			err := BuildProtoGoDb(config.ProtoDirs[i])
			if err != nil {
				log.Println(err)
			}
		}(i)

		util.Wg.Add(1)
		go func(i int) {
			defer util.Wg.Done()
			err := BuildProtoGoLogin(config.ProtoDirs[i])
			if err != nil {
				log.Println(err)
			}
		}(i)
	}
}
