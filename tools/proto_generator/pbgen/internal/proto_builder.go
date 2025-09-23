package internal

import (
	"bytes"
	"errors"
	"fmt"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"
	"log"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"pbgen/internal/config"
	"pbgen/internal/protohelper"
	"pbgen/util"
	"runtime"
	"strings"
	"sync"
)

func GenerateGameGrpcCode() error {
	util.Wg.Add(1)
	go func() {
		defer util.Wg.Done()

		err := os.MkdirAll(config.PbcProtoOutputDirectory, os.FileMode(0777))
		if err != nil {
			return
		}

		protoFile := config.GameRpcProtoPath + config.GameRpcProtoName
		sourceProtoFiles := []string{protoFile}

		// 调用 protoc 执行批量生成
		if err := generateCppFiles(sourceProtoFiles, config.PbcTempDirectory); err != nil {
			return
		}

		// 复制生成的 .pb.h 和 .pb.cc 文件到目标目录（若有变化）
		for _, protoFile := range sourceProtoFiles {
			dstFileName := strings.Replace(protoFile, config.ProtoDir, config.PbcProtoOutputDirectory, 1)
			dstFileHeadName := strings.Replace(dstFileName, config.ProtoExt, config.ProtoPbhEx, 1)
			dstFileCppName := strings.Replace(dstFileName, config.ProtoExt, config.ProtoPbcEx, 1)

			protoRelativePath := strings.Replace(protoFile, config.OutputRoot, "", 1)

			tempBaseDir := filepath.ToSlash(path.Dir(config.PbcTempDirectory + protoRelativePath))
			newBaseDir := filepath.ToSlash(path.Dir(dstFileCppName))

			tempHeaderPath := filepath.Join(tempBaseDir, filepath.Base(dstFileHeadName))
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
			if err := CopyFileIfChanged(tempHeaderPath, dstFileHeadName); err != nil {
				log.Println("copy .h failed:", err)
				continue
			}
		}

		grpcDirs := util.GetGRPCSubdirectoryNames()
		for _, dirName := range grpcDirs {

			// 为每个注册的grpc节点目录生成代码
			outputDir := config.NodeGoDirectory + dirName
			// 确保输出目录存在
			if err := os.MkdirAll(outputDir, 0755); err != nil {
				log.Println("创建输出目录失败 %s: %v", outputDir, err)
				continue
			}

			// 生成代码时传入基础go_package路径
			if err := generateGoGrpcCode(sourceProtoFiles, outputDir, config.GameRpcProtoPath); err != nil {
				log.Println("生成节点代码失败 %s: %v", outputDir, err)
				continue
			}

			// 生成机器人相关代码
			if err := os.MkdirAll(config.RobotGoOutputDirectory, 0755); err != nil {
				log.Println("创建机器人输出目录失败: %v", err)
				continue
			}
		}

	}()

	return nil
}

func BuildProtoCpp(protoPath string) error {
	// 读取 proto 文件夹内容
	fds, err := os.ReadDir(protoPath)
	if err != nil {
		return err
	}

	os.MkdirAll(config.PbcProtoOutputDirectory, os.FileMode(0777))

	var sourceProtoFiles []string
	for _, fd := range fds {
		if util.IsProtoFile(fd) {
			fullPath := filepath.ToSlash(filepath.Join(protoPath, fd.Name()))
			sourceProtoFiles = append(sourceProtoFiles, fullPath)
		}
	}

	if len(sourceProtoFiles) == 0 {
		log.Println("No .proto files found in:", protoPath)
		return nil
	}

	// 调用 protoc 执行批量生成
	if err := generateCppFiles(sourceProtoFiles, config.PbcTempDirectory); err != nil {
		return err
	}

	// 复制生成的 .pb.h 和 .pb.cc 文件到目标目录（若有变化）
	for _, protoFile := range sourceProtoFiles {
		dstFileName := strings.Replace(protoFile, config.ProtoDir, config.PbcProtoOutputDirectory, 1)
		dstFileHeadName := strings.Replace(dstFileName, config.ProtoExt, config.ProtoPbhEx, 1)
		dstFileCppName := strings.Replace(dstFileName, config.ProtoExt, config.ProtoPbcEx, 1)

		protoRelativePath := strings.Replace(protoPath, config.OutputRoot, "", 1)

		tempBaseDir := filepath.ToSlash(path.Dir(config.PbcTempDirectory + protoRelativePath))
		newBaseDir := filepath.ToSlash(path.Dir(dstFileCppName))

		tempHeaderPath := filepath.Join(tempBaseDir, filepath.Base(dstFileHeadName))
		tempCppPath := filepath.Join(tempBaseDir, filepath.Base(dstFileCppName))

		if err := os.MkdirAll(tempBaseDir, os.FileMode(0777)); err != nil {
			log.Println("mkdir failed:", err)
			continue
		}
		if err := os.MkdirAll(newBaseDir, os.FileMode(0777)); err != nil {
			log.Println("mkdir failed:", err)
			continue
		}
		if err := CopyFileIfChanged(tempCppPath, dstFileCppName); err != nil {
			log.Println("copy .cc failed:", err)
			continue
		}
		if err := CopyFileIfChanged(tempHeaderPath, dstFileHeadName); err != nil {
			log.Println("copy .h failed:", err)
			continue
		}
	}

	return nil
}

// Function to generate C++ files using protoc
func generateCppFiles(sourceProtoFiles []string, outputDir string) error {
	var cmd *exec.Cmd

	args := []string{
		"--cpp_out=" + outputDir,
	}
	args = append(args, sourceProtoFiles...) // 多个 .proto 文件一起处理
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

	if !util.HasGrpcService(strings.ToLower(protoPath)) {
		return nil
	}

	var sourceProtoFiles []string
	for _, fd := range fds {
		if util.IsProtoFile(fd) {
			sourceProtoFiles = append(sourceProtoFiles, filepath.Join(protoPath, fd.Name()))
		}
	}

	if len(sourceProtoFiles) == 0 {
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
	args = append(args, sourceProtoFiles...)
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
	for _, protoFile := range sourceProtoFiles {
		protoFile = filepath.ToSlash(protoFile)

		// 源 .proto 替换为 .pb.cc/.pb.h（你的扩展名设定）
		tempCpp := strings.Replace(protoFile, config.ProtoDir, config.GrpcTempDirectory+config.ProtoDirName, 1)
		tempCpp = strings.Replace(tempCpp, config.ProtoExt, config.GrpcPbcEx, 1)
		tempHead := strings.Replace(tempCpp, config.GrpcPbcEx, config.GrpcPbhEx, 1)

		dstCpp := strings.Replace(protoFile, config.ProtoDir, config.GrpcProtoOutputDirectory, 1)
		dstCpp = strings.Replace(dstCpp, config.ProtoExt, config.GrpcPbcEx, 1)
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

func BuildGeneratorProtoPath(dir string) string {
	// 使用filepath.Join自动处理路径分隔符，确保跨平台兼容
	return filepath.Join(
		config.GeneratorProtoDirectory,
		dir+"/",
		config.ProtoDirName,
	)
}

func BuildGeneratorGoZeroProtoPath(dir string) string {
	// 使用filepath.Join自动处理路径分隔符，确保跨平台兼容
	return filepath.Join(
		config.GeneratorProtoDirectory,
		dir+"/",
		config.GoZeroProtoDirName,
	)
}

func CopyProtoToGenDir() {
	util.Wg.Add(1)
	go func() {
		defer util.Wg.Done()
		grpcDirs := util.GetGRPCSubdirectoryNames()
		for _, dir := range grpcDirs {
			destDir := BuildGeneratorProtoPath(dir)
			err := os.MkdirAll(destDir, os.FileMode(0777))
			if err != nil {
				return
			}
			err = util.CopyLocalDir(config.ProtoDir, destDir)
			if err != nil {
				log.Fatal(err)
			}
		}

		for _, dir := range grpcDirs {
			destDir := BuildGeneratorGoZeroProtoPath(dir)
			err := os.MkdirAll(destDir, os.FileMode(0777))
			if err != nil {
				return
			}
			err = util.CopyLocalDir(config.ProtoDir, destDir)
			if err != nil {
				log.Fatal(err)
			}
		}
	}()
}

// CopyProtoToGenDir 拷贝GRPC目录并为每个文件生成对应相对路径的go_package
func AddGoPackageToProtoDir() {
	util.Wg.Add(1)
	go func() {
		defer util.Wg.Done()
		grpcDirs := util.GetGRPCSubdirectoryNames()

		for _, dirName := range grpcDirs {
			destDir := BuildGeneratorProtoPath(dirName)
			// 4. 为目录下所有文件生成对应相对路径的go_package
			// 基础路径：项目模块路径 + 原始grpc目录相对路径
			baseGoPackage := dirName
			baseGoPackage = filepath.ToSlash(baseGoPackage)

			// 处理目录下所有文件，生成动态go_package
			if err := addDynamicGoPackage(destDir, baseGoPackage, destDir, false); err != nil {
				log.Printf("❌ 处理目录 %s 的go_package失败: %v", destDir, err)
			}
		}

		for _, dirName := range grpcDirs {
			destDir := BuildGeneratorGoZeroProtoPath(dirName)
			// 4. 为目录下所有文件生成对应相对路径的go_package
			// 基础路径：项目模块路径 + 原始grpc目录相对路径
			baseGoPackage := dirName
			baseGoPackage = filepath.ToSlash(baseGoPackage)

			// 处理目录下所有文件，生成动态go_package
			if err := processFilesWithDynamicGoZeroPackage(destDir, baseGoPackage, destDir, true); err != nil {
				log.Printf("❌ 处理目录 %s 的go_package失败: %v", destDir, err)
			}

		}
	}()
}

// addDynamicGoPackage 为目录下所有文件生成基于相对路径的go_package
// rootDir: 根目录（用于计算相对路径）
// baseGoPackage: 基础go_package路径
// currentDir: 当前处理的目录
func addDynamicGoPackage(rootDir, baseGoPackage, currentDir string, isMulti bool) error {
	entries, err := os.ReadDir(currentDir)
	if err != nil {
		return err
	}

	for _, entry := range entries {
		fullPath := filepath.Join(currentDir, entry.Name())
		info, err := entry.Info()
		if err != nil {
			return err
		}

		if info.IsDir() {
			// 递归处理子目录
			if err := addDynamicGoPackage(rootDir, baseGoPackage, fullPath, isMulti); err != nil {
				return err
			}
		} else if strings.EqualFold(filepath.Ext(fullPath), ".proto") {
			// 计算文件相对根目录的路径
			relativePath, err := filepath.Rel(rootDir, filepath.Dir(fullPath))
			if err != nil {
				return err
			}

			// 生成动态go_package：基础路径 + 相对目录
			var goPackagePath string
			if relativePath == "." {
				// 文件在根目录，直接使用基础路径
				goPackagePath = baseGoPackage + config.ProtoDirName
			} else {
				// 拼接基础路径和相对目录
				goPackagePath = filepath.Join(
					baseGoPackage,
					config.ProtoDirName,
					filepath.ToSlash(relativePath),
				)
			}

			goPackagePath = filepath.ToSlash(goPackagePath)

			// 添加go_package到文件
			added, err := protohelper.AddGoPackage(fullPath, goPackagePath, isMulti)
			if err != nil {
				return err
			}
			if added {
				log.Printf("📝 为 %s 设置go_package: %s", fullPath, goPackagePath)
			} else {
				log.Printf("ℹ️ %s 已存在go_package，跳过", fullPath)
			}
		}
	}
	return nil
}

func processFilesWithDynamicGoZeroPackage(rootDir, baseGoPackage, currentDir string, isMulti bool) error {
	entries, err := os.ReadDir(currentDir)
	if err != nil {
		return err
	}

	for _, entry := range entries {
		fullPath := filepath.Join(currentDir, entry.Name())
		info, err := entry.Info()
		if err != nil {
			return err
		}

		if info.IsDir() {
			// 递归处理子目录
			if err := processFilesWithDynamicGoZeroPackage(rootDir, baseGoPackage, fullPath, isMulti); err != nil {
				return err
			}
		} else if strings.EqualFold(filepath.Ext(fullPath), ".proto") {
			// 计算文件相对根目录的路径
			relativePath, err := filepath.Rel(rootDir, filepath.Dir(fullPath))
			if err != nil {
				return err
			}

			// 生成动态go_package：基础路径 + 相对目录
			var goPackagePath string
			if relativePath == "." {
				// 文件在根目录，直接使用基础路径
				goPackagePath = baseGoPackage
			} else {
				// 拼接基础路径和相对目录
				goPackagePath = filepath.Join(
					baseGoPackage,
					filepath.ToSlash(relativePath),
				)
			}

			goPackagePath = filepath.ToSlash(goPackagePath)

			// 添加go_package到文件
			added, err := protohelper.AddGoPackage(fullPath, goPackagePath, isMulti)
			if err != nil {
				return err
			}
			if added {
				log.Printf("📝 为 %s 设置go_package: %s", fullPath, goPackagePath)
			} else {
				log.Printf("ℹ️ %s 已存在go_package，跳过", fullPath)
			}
		}
	}
	return nil
}

func generateGoGrpcCode(sourceProtoFiles []string, outputDir string, protoRootPath string) error {
	// 提前校验空输入
	if len(sourceProtoFiles) == 0 {
		return fmt.Errorf("protoFiles不能为空")
	}

	// 3. 构建protoc命令参数
	args := []string{
		"--go_out=" + outputDir,
		"--go-grpc_out=" + outputDir,
		"--proto_path=" + protoRootPath,
		"--proto_path=" + config.ProtoBufferDirectory,
	}

	args = append(args, sourceProtoFiles...)

	// 6. 执行命令
	cmd := exec.Command(config.ProtocPath, args...)
	var out, stderr bytes.Buffer
	cmd.Stdout = &out
	cmd.Stderr = &stderr

	log.Printf("执行protoc命令: %s", cmd.String())

	if err := cmd.Run(); err != nil {
		log.Printf("protoc错误输出: %s", stderr.String())
		return fmt.Errorf("protoc执行失败: %v", err)
	}

	// 输出完整的生成信息
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

// GenerateGoGRPCFromProto 递归处理指定目录下所有proto文件并生成Go gRPC代码
func GenerateGoGRPCFromProto(rootDir string) error {
	if util.CheckEtcdServiceExistence(rootDir) {
		return nil
	}

	// 递归收集所有proto文件
	var sourceProtoFiles []string
	err := filepath.WalkDir(rootDir, func(path string, d os.DirEntry, err error) error {
		if err != nil {
			return fmt.Errorf("访问路径失败 %s: %v", path, err)
		}

		// 跳过目录和数据库proto文件
		if d.IsDir() || !util.IsProtoFile(d) || d.Name() == config.DbProtoFileName {
			return nil
		}

		// 收集符合条件的proto文件（转换为斜杠路径）
		sourceProtoFiles = append(sourceProtoFiles, filepath.ToSlash(path))
		return nil
	})

	if err != nil {
		return fmt.Errorf("递归遍历目录失败 %s: %v", rootDir, err)
	}

	// 无proto文件时直接返回
	if len(sourceProtoFiles) == 0 {
		log.Printf("目录 %s 下没有需要处理的proto文件", rootDir)
		return nil
	}

	// 为每个注册的grpc节点目录生成代码
	outputDir := config.NodeGoDirectory
	// 确保输出目录存在
	if err := os.MkdirAll(outputDir, 0755); err != nil {
		return fmt.Errorf("创建输出目录失败 %s: %v", outputDir, err)
	}

	rootDir = filepath.Dir(rootDir)
	rootDir = filepath.ToSlash(rootDir)

	// 生成代码时传入基础go_package路径
	if err := generateGoGrpcCode(sourceProtoFiles, outputDir, rootDir); err != nil {
		return fmt.Errorf("生成节点代码失败 %s: %v", outputDir, err)
	}

	// 生成机器人相关代码
	if err := os.MkdirAll(config.RobotGoOutputDirectory, 0755); err != nil {
		return fmt.Errorf("创建机器人输出目录失败: %v", err)
	}

	return nil
}

// BuildGrpcServiceProto 并发处理所有GRPC目录，递归遍历每个目录下的proto文件
func BuildGrpcServiceProto() {
	grpcDirs := util.GetGRPCSubdirectoryNames()
	var wg sync.WaitGroup

	for _, dirName := range grpcDirs {
		wg.Add(1)

		// 传递当前目录名副本到goroutine，避免循环变量捕获问题
		go func(currentDir string) {
			defer wg.Done()

			// 构建目标目录路径
			destDir := BuildGeneratorProtoPath(currentDir)
			if _, err := os.Stat(destDir); errors.Is(err, os.ErrNotExist) {
				log.Printf("目录 %s 不存在，跳过处理", destDir)
				return
			}

			// 递归处理该目录下所有proto文件
			if err := GenerateGoGRPCFromProto(destDir); err != nil {
				log.Printf("处理目录 %s 失败: %v", currentDir, err)
			} else {
				log.Printf("目录 %s 处理完成", currentDir)
			}
		}(dirName)
	}

	wg.Wait()
	log.Println("所有GRPC服务proto文件递归处理完成")
}
