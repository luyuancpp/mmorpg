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
	var cmd *exec.Cmd

	args := []string{
		"--cpp_out=" + outputDir,
	}
	args = append(args, protoFiles...) // 多个 .proto 文件一起处理
	args = append(args,
		"-I="+config.ProtoParentIncludePathDir,
		"--proto_path="+config.ProtoBufferDirectory,
	)

	cmd = exec.Command("protoc", args...)

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
	cmd = exec.Command("protoc", args...)

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

func CollectProtoFiles() []string {
	// 遍历 config.ProtoDirs 中的每个目录
	for _, protoPath := range config.ProtoDirs {
		// 1. 读取 protoPath 目录下的所有文件
		files, err := os.ReadDir(protoPath)
		if err != nil {
			return nil
		}

		// 2. 筛选有效的 .proto 文件并排除 config.DbProtoFileName
		for _, file := range files {
			// 跳过非 .proto 文件和 config.DbProtoFileName
			if !util.IsProtoFile(file) || file.Name() == config.DbProtoFileName {
				continue
			}

			// 将符合条件的文件添加到 protoFiles 中
			fullPath := filepath.ToSlash(filepath.Join(protoPath, file.Name()))
			ProtoFiles = append(ProtoFiles, fullPath)
		}

		// 如果该目录没有符合条件的 .proto 文件，记录日志但不返回错误
		if len(ProtoFiles) == 0 {
			log.Printf("No valid proto files found in path: %s", protoPath)
		}
	}

	// 如果 protoFiles 为空，表示没有任何有效的 .proto 文件
	if len(ProtoFiles) == 0 {
		return nil
	}

	return ProtoFiles
}

func generateGoProto(protoFiles []string, outputDir string) error {
	// 1. 强制转换所有路径为绝对路径（核心：摆脱当前工作目录影响）
	outputAbsDir, err := filepath.Abs(outputDir)
	if err != nil {
		return fmt.Errorf("无法获取输出目录绝对路径: %v", err)
	}

	// 获取proto根目录的绝对路径（从配置中读取）
	protoParentAbsDir, err := filepath.Abs(config.ProtoParentIncludePathDir)
	if err != nil {
		return fmt.Errorf("无法获取proto父目录绝对路径: %v", err)
	}
	protoBufferAbsDir, err := filepath.Abs(config.ProtoBufferDirectory)
	if err != nil {
		return fmt.Errorf("无法获取proto缓存目录绝对路径: %v", err)
	}

	// 2. 计算模块名（与输出目录的绝对路径强关联）
	// 例如：outputAbsDir = "D:/game/mmorpg1/go/db" → 模块名 = "db/pb/game"
	moduleName := filepath.Base(outputAbsDir) + "/pb"

	// 3. 构建protoc命令参数（全部使用绝对路径）
	args := []string{
		"--go_out=" + outputAbsDir,          // 生成文件的绝对输出目录
		"--go_opt=module=" + moduleName,     // 模块名与输出目录绑定
		"--go_opt=paths=import",             // 按模块路径生成导入语句
		"--proto_path=" + protoParentAbsDir, // proto查找根目录1（绝对路径）
		"--proto_path=" + protoBufferAbsDir, // proto查找根目录2（绝对路径）
	}

	// 4. 为每个proto文件生成正确的M映射（关键：路径必须从--proto_path根目录出发）
	for _, protoFile := range ProtoFiles {
		// 转换proto文件为绝对路径
		protoAbsFile, err := filepath.Abs(protoFile)
		if err != nil {
			return fmt.Errorf("无法获取proto文件绝对路径 %s: %v", protoFile, err)
		}

		// 计算proto文件相对于某个--proto_path根目录的相对路径
		var relativeProtoPath string
		for _, root := range []string{protoParentAbsDir, protoBufferAbsDir} {
			if strings.HasPrefix(protoAbsFile, root) {
				relativeProtoPath, err = filepath.Rel(root, protoAbsFile)
				if err == nil && relativeProtoPath != "" {
					break // 找到匹配的根目录，跳出循环
				}
			}
		}

		if relativeProtoPath == "" {
			return fmt.Errorf("proto文件 %s 不在任何--proto_path目录下", protoAbsFile)
		}

		// 统一路径分隔符为'/'（protoc要求，跨平台兼容）
		relativeProtoPath = strings.ReplaceAll(relativeProtoPath, "\\", "/")
		// 添加M映射：相对路径 → 模块路径
		args = append(args, "--go_opt=M"+relativeProtoPath+"="+moduleName)
	}

	// 5. 替换proto文件列表为绝对路径
	absProtoFiles := make([]string, len(protoFiles))
	for i, f := range protoFiles {
		absF, err := filepath.Abs(f)
		if err != nil {
			return fmt.Errorf("无法转换proto文件为绝对路径 %s: %v", f, err)
		}
		absProtoFiles[i] = absF
	}
	args = append(args, absProtoFiles...)

	// 6. 选择protoc命令（使用绝对路径调用protoc更可靠）
	protocPath := "protoc" // 默认Linux路径

	// 7. 执行命令
	cmd := exec.Command(protocPath, args...)
	var out, stderr bytes.Buffer
	cmd.Stdout = &out
	cmd.Stderr = &stderr

	// 打印执行的命令（便于调试）
	log.Printf("执行protoc命令: %s", cmd.String())

	if err := cmd.Run(); err != nil {
		log.Printf("protoc错误输出: %s", stderr.String())
		return fmt.Errorf("protoc执行失败: %v", err)
	}

	// 输出最终生成路径（确保用户可见）
	log.Printf("生成成功！文件位于: %s", outputAbsDir)
	return nil
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

	err = generateGoProto(protoFiles, config.RobotGoOutputDirectory)
	if err != nil {
		return err
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
