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
	"pbgen/internal/config"
	"pbgen/util"
	"runtime"
	"strings"
)

func BuildProtoCpp(protoPath string) error {
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

func BuildProtoGrpcCpp(protoPath string) error {
	// 读取 proto 目录文件
	fds, err := os.ReadDir(protoPath)
	if err != nil {
		return err
	}

	os.MkdirAll(config.GrpcTempDirectory, os.FileMode(0777))
	os.MkdirAll(config.GrpcOutputDirectory, os.FileMode(0777))

	basePath := strings.ToLower(path.Base(protoPath))
	if !util.HasGrpcService(basePath) {
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

func generateGoProto(protoFiles []string, outputDir string) error {
	sysType := runtime.GOOS
	var cmd *exec.Cmd

	// 提取模块名称（即目标 Go 项目的名称）
	moduleName := strings.ToLower(path.Base(outputDir)) + "/pb/game"

	// 生成 protoc 命令的参数
	args := []string{
		"--go_out=" + outputDir,         // 生成的 Go 文件输出目录
		"--go_opt=module=" + moduleName, // 模块路径
		"--go_opt=paths=import",         // 使用 import 路径而非 source_relative
		"--proto_path=" + config.ProtoParentIncludePathDir,
		"--proto_path=" + config.ProtoBufferDirectory,
	}

	// 为每个 proto 文件生成正确的 M 选项映射
	for _, protoFile := range protoFiles {
		// 关键修复：计算 proto 文件相对于 proto_path 的相对路径
		// 优先尝试与第一个 proto_path 匹配
		relPath, err := getRelativeToProtoPath(protoFile, config.ProtoParentIncludePathDir)
		if err != nil {
			// 若不匹配第一个，则尝试与第二个 proto_path 匹配
			relPath, err = getRelativeToProtoPath(protoFile, config.ProtoBufferDirectory)
			if err != nil {
				return fmt.Errorf("proto file %s not in any proto_path", protoFile)
			}
		}

		// M选项格式：M{proto_path相对路径}={目标包路径}
		args = append(args, "--go_opt=M"+relPath+"="+moduleName)
	}

	// 添加 proto 文件列表（使用绝对路径或正确的相对路径）
	args = append(args, protoFiles...)

	// 根据操作系统选择正确的 protoc 命令
	if sysType == "linux" {
		cmd = exec.Command("protoc", args...)
	} else {
		cmd = exec.Command("./protoc.exe", args...)
	}

	// 捕获标准输出和错误输出
	var out, stderr bytes.Buffer
	cmd.Stdout = &out
	cmd.Stderr = &stderr

	// 打印命令日志（替换空格为换行，便于调试长命令）
	log.Println("Running command:")
	log.Println(strings.ReplaceAll(cmd.String(), " ", "\n"))

	// 执行 protoc 命令
	if err := cmd.Run(); err != nil {
		// 打印 stderr 输出，帮助调试
		log.Println("protoc stderr:", stderr.String())
		return fmt.Errorf("protoc error: %v", err)
	}

	// 打印标准输出，帮助调试
	log.Println("protoc stdout:", out.String())

	return nil
}

// 辅助函数：计算 proto 文件相对于 proto_path 的相对路径
func getRelativeToProtoPath(protoFile, protoPath string) (string, error) {
	// 将 proto_path 和 protoFile 转换为绝对路径，消除相对路径歧义
	absProtoPath, err := filepath.Abs(protoPath)
	if err != nil {
		return "", err
	}
	absProtoFile, err := filepath.Abs(protoFile)
	if err != nil {
		return "", err
	}

	// 检查 protoFile 是否在 protoPath 目录下
	if !strings.HasPrefix(absProtoFile, absProtoPath) {
		return "", fmt.Errorf("file not in proto_path")
	}

	// 计算相对路径（从 proto_path 到 protoFile）
	relPath, err := filepath.Rel(absProtoPath, absProtoFile)
	if err != nil {
		return "", err
	}

	// 将路径分隔符统一转换为 '/'（protoc 要求）
	return strings.ReplaceAll(relPath, "\\", "/"), nil
}

// GenerateGoGRPCFromProto processes .proto files in the given directory
func GenerateGoGRPCFromProto(protoPath string) error {
	if !util.HasGrpcService(protoPath) {
		return nil
	}

	if util.CheckEtcdServiceExistence(protoPath) {
		return nil
	}

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

		fullPath := filepath.ToSlash(filepath.Join(protoPath, file.Name()))
		protoFiles = append(protoFiles, fullPath)
	}

	// 3. 如果没有符合条件的 proto 文件，记录日志并退出
	if len(protoFiles) == 0 {
		log.Println("No proto files to process for login:", protoPath)
		return nil
	}

	// 4. 为所有注册的 grpc 节点目录生成 Go gRPC 代码
	for i := 0; i < len(config.ProtoDirs); i++ {
		if !util.HasGrpcService(config.ProtoDirs[i]) {
			continue
		}
		basePath := strings.ToLower(path.Base(config.ProtoDirs[i]))
		outputDir := config.NodeGoDirectory + basePath
		err := generateGoProto(protoFiles, outputDir)
		if err != nil {
			return err
		}
	}

	return nil
}

func BuildProtoGo(protoPath string) error {
	if util.HasGrpcService(protoPath) {
		return nil
	}
	if util.CheckEtcdServiceExistence(protoPath) {
		return nil
	}

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
		fullPath := filepath.ToSlash(filepath.Join(protoPath, fd.Name()))
		protoFiles = append(protoFiles, fullPath)
	}

	if len(protoFiles) == 0 {
		log.Println("No proto files to process for login:", protoPath)
		return nil
	}

	for i := 0; i < len(config.ProtoDirs); i++ {
		if !util.HasGrpcService(config.ProtoDirs[i]) {
			continue
		}
		basePath := strings.ToLower(path.Base(config.ProtoDirs[i]))
		outputDir := config.NodeGoDirectory + basePath
		err := generateGoProto(protoFiles, outputDir)
		if err != nil {
			return err
		}
	}

	return nil
}

func generateRobotGoProto(protoFiles []string, outputDir string) error {
	sysType := runtime.GOOS
	var cmd *exec.Cmd

	moduleName := strings.ToLower(path.Base(outputDir))

	args := []string{
		"--go_out=" + outputDir,
		"--go_opt=module=" + moduleName, // 使用正确的模块名
		"--proto_path=" + config.ProtoParentIncludePathDir,
		"--proto_path=" + config.ProtoBufferDirectory,
	}
	args = append(args, protoFiles...) // proto文件放在最后

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

		// 步骤1：收集目标 proto 文件（去重，确保只包含需要的文件）
		var allProtoFiles []string
		protoFileSet := make(map[string]struct{}) // 用于去重
		for _, dir := range config.ProtoDirs {
			fds, err := os.ReadDir(dir)
			if err != nil {
				log.Printf("警告：读取目录 %s 失败，跳过: %v", dir, err)
				continue
			}
			for _, fd := range fds {
				if util.IsProtoFile(fd) {
					// 转换为绝对路径，避免同名文件重复
					absPath, err := filepath.Abs(filepath.Join(dir, fd.Name()))
					if err != nil {
						log.Printf("警告：获取绝对路径失败 %s，跳过: %v", fd.Name(), err)
						continue
					}
					absPath = filepath.ToSlash(absPath)
					if _, exists := protoFileSet[absPath]; !exists {
						protoFileSet[absPath] = struct{}{}
						allProtoFiles = append(allProtoFiles, absPath)
					}
				}
			}
		}

		if len(allProtoFiles) == 0 {
			log.Println("No proto files found in any directory")
			return
		}
		log.Printf("共收集到 %d 个唯一 proto 文件", len(allProtoFiles))

		// 步骤2：构建 protoc 命令参数（核心：先加选项和导入路径，后加 proto 文件）
		descOut := filepath.ToSlash(config.AllInOneProtoDescFile)
		// 1. 基础选项（输出路径、包含依赖、包含源码信息）
		args := []string{
			"--descriptor_set_out=" + descOut,
			"--include_imports",     // 必须：包含所有依赖的描述符
			"--include_source_info", // 建议：包含源码信息，便于调试
		}
		// 2. 导入路径（必须在 proto 文件之前）
		importPaths := []string{
			config.ProtoParentIncludePathDir,
			config.ProtoBufferDirectory,
		}
		for _, ip := range importPaths {
			if ip != "" { // 跳过空路径
				absIP, err := filepath.Abs(ip)
				if err != nil {
					log.Printf("警告：导入路径 %s 无效，跳过: %v", ip, err)
					continue
				}
				args = append(args, "--proto_path="+filepath.ToSlash(absIP))
			}
		}
		// 3. 目标 proto 文件（最后加）
		args = append(args, allProtoFiles...)

		// 步骤3：执行 protoc 命令（适配系统）
		var cmd *exec.Cmd
		protocPath := "protoc" // 默认 Linux 路径
		if runtime.GOOS != "linux" {
			// Windows：确保 protoc.exe 路径正确（建议使用绝对路径）
			protocPath = "./protoc.exe"
			// 可选：检查 protoc.exe 是否存在
			if _, err := os.Stat(protocPath); err != nil {
				log.Fatalf("protoc.exe 不存在于路径 %s: %v", protocPath, err)
			}
		}
		cmd = exec.Command(protocPath, args...)

		// 捕获输出，便于调试
		var out, stderr bytes.Buffer
		cmd.Stdout = &out
		cmd.Stderr = &stderr

		log.Printf("执行 protoc 命令: %s %s", cmd.Path, strings.Join(cmd.Args[1:], " "))
		if err := cmd.Run(); err != nil {
			log.Fatalf("protoc 执行失败: 错误=%v,  stderr=%s", err, stderr.String())
		}
		log.Printf("protoc 执行成功: stdout=%s", out.String())

		// 步骤4：读取并解析描述符文件
		data, err := os.ReadFile(config.AllInOneProtoDescFile)
		if err != nil {
			log.Fatalf("读取描述符文件失败: %v", err)
		}
		log.Printf("描述符文件大小: %d 字节（非空，说明生成成功）", len(data))

		// 解析为 FdSet（确保 FdSet 已初始化）
		if FdSet == nil {
			FdSet = &descriptorpb.FileDescriptorSet{}
		}
		if err := proto.Unmarshal(data, FdSet); err != nil {
			log.Fatalf("解析描述符文件失败: %v，可能是文件损坏或版本不兼容", err)
		}

		// 验证：打印加载的文件数和消息数（确保包含目标文件）
		log.Printf("成功解析描述符：包含 %d 个文件", len(FdSet.GetFile()))
		for _, fileDesc := range FdSet.GetFile() {
			// 只打印目标文件的消息数（替换为你的目标文件前缀）
			if strings.Contains(fileDesc.GetName(), "proto/service/go/grpc") ||
				strings.Contains(fileDesc.GetName(), "proto/common") {
				log.Printf("  文件 %s: 包含 %d 个消息", fileDesc.GetName(), len(fileDesc.GetMessageType()))
			}
		}

		log.Printf("描述符文件生成路径: %s", descOut)
	}()
}

func BuildAllProtoc() {
	// Iterate over configured proto directories
	for i := 0; i < len(config.ProtoDirs); i++ {
		if util.HasGrpcService(config.ProtoDirs[i]) {
			continue
		}

		util.Wg.Add(1)
		go func(i int) {
			defer util.Wg.Done()
			// Execute functions concurrently for each directory
			err := BuildProtoCpp(config.ProtoDirs[i])
			if err != nil {
				log.Println(err)
			}
		}(i)

		util.Wg.Add(1)
		go func(i int) {
			defer util.Wg.Done()
			err := BuildProtoGrpcCpp(config.ProtoDirs[i])
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

	}
}

func BuildGrpcServiceProto() {
	// Iterate over configured proto directories
	for i := 0; i < len(config.ProtoDirs); i++ {
		util.Wg.Add(1)
		go func(i int) {
			defer util.Wg.Done()
			err := BuildProtoGo(config.ProtoDirs[i])
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
	}
}
