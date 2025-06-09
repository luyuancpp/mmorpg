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

		protoRelativePath := strings.Replace(protoPath, config.ProjectDir, "", 1)
		newBaseDir := filepath.ToSlash(path.Dir(config.PbcTempDirectory + protoRelativePath))

		tempHeadFileName := filepath.Join(newBaseDir, filepath.Base(dstFileHeadName))
		tempCppFileName := filepath.Join(newBaseDir, filepath.Base(dstFileCppName))

		if err := os.MkdirAll(newBaseDir, os.FileMode(0777)); err != nil {
			log.Println("mkdir failed:", err)
			continue
		}

		if err := CopyFileIfChanged(dstFileCppName, tempCppFileName); err != nil {
			log.Println("copy .cc failed:", err)
			continue
		}
		if err := CopyFileIfChanged(dstFileHeadName, tempHeadFileName); err != nil {
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

	// 构造 protoc 命令
	sysType := runtime.GOOS
	var cmd *exec.Cmd
	if sysType == `linux` {
		args := []string{
			"--grpc_out=" + config.GrpcTempDirectory,
			"--plugin=protoc-gen-grpc=grpc_cpp_plugin",
		}
		args = append(args, protoFiles...)
		args = append(args,
			"--proto_path="+config.ProtoParentIncludePathDir,
			"--proto_path="+config.ProtoBufferDirectory,
		)
		cmd = exec.Command("protoc", args...)
	} else {
		args := []string{
			"--grpc_out=" + config.GrpcTempDirectory,
			"--plugin=protoc-gen-grpc=grpc_cpp_plugin.exe",
		}
		args = append(args, protoFiles...)
		args = append(args,
			"--proto_path="+config.ProtoParentIncludePathDir,
			"--proto_path="+config.ProtoBufferDirectory,
		)
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
		md5FileName := strings.Replace(protoFile, config.ProtoDir, config.GrpcTempDirectory+config.ProtoDirName, 1)
		md5FileName = strings.Replace(md5FileName, config.ProtoEx, config.GrpcPbcEx, 1)

		dstFileName := strings.Replace(protoFile, config.ProtoDir, config.GrpcProtoOutputDirectory, 1)
		dstFileName = strings.Replace(dstFileName, config.ProtoEx, config.GrpcPbcEx, 1)

		// 创建目录
		dir := path.Dir(md5FileName)
		if err := os.MkdirAll(dir, os.FileMode(0777)); err != nil {
			log.Fatal(err)
			return err
		}

		// 拷贝文件（如内容有变）
		if err := CopyFileIfChanged(md5FileName, dstFileName); err != nil {
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

func BuildProtoDesc(protoPath string) error {
	// 读取 proto 文件
	fds, err := os.ReadDir(protoPath)
	if err != nil {
		return err
	}

	os.MkdirAll(config.PbDescDirectory, os.FileMode(0777))

	var protoFiles []string
	for _, fd := range fds {
		if util.IsProtoFile(fd) {
			protoFiles = append(protoFiles, filepath.ToSlash(filepath.Join(protoPath, fd.Name())))
		}
	}

	// 按文件处理（顺序执行，避免并发 fork）
	for _, fileName := range protoFiles {
		// 构造输出路径
		baseName := filepath.Base(fileName)
		descOut := filepath.ToSlash(filepath.Join(config.PbDescDirectory, baseName+config.ProtoDescExtension))

		var cmd *exec.Cmd
		if runtime.GOOS == "linux" {
			cmd = exec.Command("protoc",
				"--descriptor_set_out="+descOut,
				fileName,
				"--proto_path="+config.ProtoParentIncludePathDir,
				"--proto_path="+config.ProtoBufferDirectory)
		} else {
			cmd = exec.Command("./protoc.exe",
				"--descriptor_set_out="+descOut,
				fileName,
				"--proto_path="+config.ProtoParentIncludePathDir,
				"--proto_path="+config.ProtoBufferDirectory)
		}

		var out, stderr bytes.Buffer
		cmd.Stdout = &out
		cmd.Stderr = &stderr

		log.Println("Running:", cmd.String())
		if err := cmd.Run(); err != nil {
			fmt.Println("protoc error:", stderr.String())
			return fmt.Errorf("failed to run protoc: %w", err)
		}
	}

	return nil
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

func generateDeployGoProto(protoFiles []string, outputDir string) error {
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

func BuildProtoGoDeploy(protoPath string) error {
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

		if fd.Name() == config.DbProtoFileName ||
			fd.Name() == config.GameMysqlDBProtoFileName ||
			fd.Name() == config.LoginServiceProtoFileName {
			continue
		}

		if !strings.Contains(protoPath, config.ProtoDirectoryNames[config.CommonProtoDirIndex]) {
			continue // 修复 return 错误
		}

		fullPath := filepath.ToSlash(filepath.Join(protoPath, fd.Name()))
		protoFiles = append(protoFiles, fullPath)
	}

	if len(protoFiles) == 0 {
		log.Println("No proto files to deploy for:", protoPath)
		return nil
	}

	return generateDeployGoProto(protoFiles, config.DeployDirectory)
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
