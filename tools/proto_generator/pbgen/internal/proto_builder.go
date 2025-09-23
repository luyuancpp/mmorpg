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
	"path/filepath"
	"pbgen/internal/config"
	"pbgen/internal/protohelper"
	"pbgen/util"
	"runtime"
	"strings"
	"sync"
)

// ------------------------------
// 基础工具函数（跨模块复用）
// ------------------------------

// ensureDirExist 确保目录存在，不存在则创建（权限默认0755）
func ensureDirExist(dirPath string) error {
	if err := os.MkdirAll(dirPath, 0755); err != nil {
		return fmt.Errorf("创建目录 %s 失败: %w", dirPath, err)
	}
	return nil
}

// collectProtoFiles 收集指定目录下所有后缀为.proto的文件（返回绝对路径）
func collectProtoFiles(dirPath string) ([]string, error) {
	fds, err := os.ReadDir(dirPath)
	if err != nil {
		return nil, fmt.Errorf("读取目录 %s 失败: %w", dirPath, err)
	}

	var protoFiles []string
	for _, fd := range fds {
		if util.IsProtoFile(fd) {
			absPath, err := filepath.Abs(filepath.Join(dirPath, fd.Name()))
			if err != nil {
				log.Printf("获取文件 %s 绝对路径失败: %v，跳过该文件", fd.Name(), err)
				continue
			}
			protoFiles = append(protoFiles, absPath)
		}
	}
	return protoFiles, nil
}

// execProtocCommand 执行protoc命令（默认使用系统PATH中的protoc）
func execProtocCommand(args []string, actionDesc string) error {
	cmd := exec.Command("protoc", args...)
	return runProtocCmd(cmd, actionDesc)
}

// execProtocCommandWithPath 执行protoc命令（指定protoc路径，适配自定义安装场景）
func execProtocCommandWithPath(protocPath string, args []string, actionDesc string) error {
	cmd := exec.Command(protocPath, args...)
	return runProtocCmd(cmd, actionDesc)
}

// runProtocCmd 执行protoc命令并处理输出和错误（底层复用逻辑）
func runProtocCmd(cmd *exec.Cmd, actionDesc string) error {
	var out, stderr bytes.Buffer
	cmd.Stdout = &out
	cmd.Stderr = &stderr

	log.Printf("执行 %s: %s", actionDesc, cmd.String())
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("protoc执行失败: 错误=%v, 错误输出=%s", err, stderr.String())
	}

	if out.Len() > 0 {
		log.Printf("%s 成功，输出: %s", actionDesc, out.String())
	}
	return nil
}

// copyProtoDirToDest 拷贝Proto目录到目标目录（保持目录结构）
func copyProtoDirToDest(srcDir, destDir string) error {
	if err := ensureDirExist(destDir); err != nil {
		return err
	}
	if err := util.CopyLocalDir(srcDir, destDir); err != nil {
		return fmt.Errorf("拷贝目录 %s -> %s 失败: %w", srcDir, destDir, err)
	}
	log.Printf("目录 %s 拷贝到 %s 成功", srcDir, destDir)
	return nil
}

// ------------------------------
// C++ 代码生成相关
// ------------------------------

// GenerateGameGrpcCode 生成游戏相关的GRPC代码（含C++序列化代码和Go节点代码）
// generateGameGrpcGoCode 生成游戏GRPC的Go节点代码（多节点适配）
func generateGameGrpcGoCode(sourceProtoFiles []string) error {
	// 1. 获取所有GRPC节点目录
	grpcDirs := util.GetGRPCSubdirectoryNames()
	if len(grpcDirs) == 0 {
		log.Println("未找到任何GRPC节点目录，跳过Go代码生成")
		return nil
	}

	// 2. 为每个节点生成专属GRPC代码
	for _, dirName := range grpcDirs {
		// 构建节点输出目录（确保目录结构正确）
		nodeOutputDir := filepath.Join(config.NodeGoDirectory, dirName)
		if err := ensureDirExist(nodeOutputDir); err != nil {
			log.Printf("创建节点目录 %s 失败: %v，跳过该节点", nodeOutputDir, err)
			continue
		}

		// 生成该节点的Go GRPC代码（使用游戏Proto根路径）
		if err := generateGoGrpcCode(
			sourceProtoFiles,
			nodeOutputDir,
			config.GameRpcProtoPath,
		); err != nil {
			log.Printf("生成节点 %s 的Go GRPC代码失败: %v，跳过该节点", dirName, err)
			continue
		}
		log.Printf("节点 %s 的Go GRPC代码生成成功", dirName)
	}

	// 3. 确保机器人代码输出目录存在（为后续机器人代码生成做准备）
	if err := ensureDirExist(config.RobotGoOutputDirectory); err != nil {
		return fmt.Errorf("创建机器人代码目录失败: %w", err)
	}

	log.Println("所有游戏GRPC节点的Go代码生成完成")
	return nil
}

func GenerateGameGrpcCode() error {
	util.Wg.Add(1)
	go func() {
		defer util.Wg.Done()

		// 1. 准备游戏Proto文件路径
		gameProtoPath := filepath.Join(config.GameRpcProtoPath, config.GameRpcProtoName)
		sourceProtoFiles := []string{gameProtoPath}

		// 2. 生成C++序列化代码并拷贝到目标目录
		if err := ensureDirExist(config.PbcProtoOutputDirectory); err != nil {
			log.Printf("创建游戏C++输出目录失败: %v", err)
			return
		}
		if err := generateCppCode(sourceProtoFiles, config.PbcTempDirectory); err != nil {
			log.Printf("生成游戏C++代码失败: %v", err)
			return
		}
		if err := copyCppGeneratedFiles(sourceProtoFiles, config.PbcTempDirectory, config.PbcProtoOutputNoProtoSuffixPath); err != nil {
			log.Printf("拷贝游戏C++代码失败: %v", err)
			return
		}

		// 3. 生成Go语言GRPC节点代码
		if err := generateGameGrpcGoCode(sourceProtoFiles); err != nil {
			log.Printf("生成游戏GRPC Go代码失败: %v", err)
			return
		}
	}()

	return nil
}

// BuildProtoCpp 批量生成指定目录下Proto文件的C++序列化代码
func BuildProtoCpp(protoDir string) error {
	// 1. 收集Proto文件
	sourceProtoFiles, err := collectProtoFiles(protoDir)
	if err != nil {
		return fmt.Errorf("收集Proto文件失败: %w", err)
	}
	if len(sourceProtoFiles) == 0 {
		log.Printf("目录 %s 下无Proto文件，跳过处理", protoDir)
		return nil
	}

	// 2. 生成并拷贝C++代码
	if err := ensureDirExist(config.PbcProtoOutputDirectory); err != nil {
		return fmt.Errorf("创建C++输出目录失败: %w", err)
	}
	if err := generateCppCode(sourceProtoFiles, config.PbcTempDirectory); err != nil {
		return fmt.Errorf("生成C++代码失败: %w", err)
	}
	if err := copyCppGeneratedFiles(sourceProtoFiles, config.PbcTempDirectory, config.PbcProtoOutputDirectory); err != nil {
		return fmt.Errorf("拷贝C++代码失败: %w", err)
	}

	return nil
}

// generateCppCode 调用protoc生成C++序列化代码（底层实现）
func generateCppCode(sourceProtoFiles []string, outputDir string) error {
	// 确保输出目录存在
	if err := ensureDirExist(outputDir); err != nil {
		return err
	}

	// 转换所有路径为绝对路径，避免protoc路径匹配问题
	absOutputDir, err := filepath.Abs(outputDir)
	if err != nil {
		return fmt.Errorf("获取输出目录绝对路径失败: %w", err)
	}

	absOutputRoot, err := filepath.Abs(config.OutputRoot)
	if err != nil {
		return fmt.Errorf("获取OutputRoot绝对路径失败: %w", err)
	}

	absProtoBufferDir, err := filepath.Abs(config.ProtoBufferDirectory)
	if err != nil {
		return fmt.Errorf("获取ProtoBufferDirectory绝对路径失败: %w", err)
	}

	// 构建protoc参数（全部使用绝对路径）
	args := []string{
		fmt.Sprintf("--cpp_out=%s", absOutputDir),
		fmt.Sprintf("--proto_path=%s", absOutputRoot),
		fmt.Sprintf("--proto_path=%s", absProtoBufferDir),
	}

	// 确保源文件路径都是绝对路径（补充校验）
	for _, file := range sourceProtoFiles {
		if !filepath.IsAbs(file) {
			// 转换相对路径为绝对路径
			absFile, err := filepath.Abs(file)
			if err != nil {
				log.Printf("文件 %s 不是绝对路径且转换失败，跳过: %v", file, err)
				continue
			}
			args = append(args, absFile)
		} else {
			args = append(args, file)
		}
	}

	return execProtocCommand(args, "生成C++序列化代码")
}

// copyCppGeneratedFiles 将临时目录的C++生成文件（.pb.h/.pb.cc）拷贝到目标目录
func copyCppGeneratedFiles(sourceProtoFiles []string, tempDir, destDir string) error {
	for _, protoFile := range sourceProtoFiles {
		// 计算Proto文件相对根目录的路径（保持目录结构一致）
		protoRelPath, err := filepath.Rel(config.OutputRoot, protoFile)
		if err != nil {
			log.Printf("计算Proto相对路径失败 %s: %v，跳过该文件", protoFile, err)
			continue
		}

		// 构建文件路径（.proto -> .pb.h/.pb.cc）
		protoFileName := filepath.Base(protoFile)
		headerFileName := strings.Replace(protoFileName, config.ProtoExt, config.ProtoPbhEx, 1)
		cppFileName := strings.Replace(protoFileName, config.ProtoExt, config.ProtoPbcEx, 1)

		// 临时文件路径和目标文件路径
		tempHeaderPath := filepath.Join(tempDir, filepath.Dir(protoRelPath), headerFileName)
		tempCppPath := filepath.Join(tempDir, filepath.Dir(protoRelPath), cppFileName)
		destHeaderPath := filepath.Join(destDir, filepath.Dir(protoRelPath), headerFileName)
		destCppPath := filepath.Join(destDir, filepath.Dir(protoRelPath), cppFileName)

		// 拷贝文件
		if err := CopyFileIfChanged(tempHeaderPath, destHeaderPath); err != nil {
			log.Printf("拷贝头文件失败 %s: %v，跳过该文件", protoFile, err)
			continue
		}
		if err := CopyFileIfChanged(tempCppPath, destCppPath); err != nil {
			log.Printf("拷贝实现文件失败 %s: %v，跳过该文件", protoFile, err)
			continue
		}
	}
	return nil
}

// ------------------------------
// C++ GRPC 代码生成相关
// ------------------------------

// BuildProtoGrpcCpp 生成指定目录下Proto文件的C++ GRPC服务代码
func BuildProtoGrpcCpp(protoDir string) error {
	// 1. 检查是否包含GRPC服务定义
	if !util.HasGrpcService(strings.ToLower(protoDir)) {
		log.Printf("目录 %s 无GRPC服务定义，跳过处理", protoDir)
		return nil
	}

	// 2. 收集Proto文件
	sourceProtoFiles, err := collectProtoFiles(protoDir)
	if err != nil {
		return fmt.Errorf("收集GRPC Proto文件失败: %w", err)
	}
	if len(sourceProtoFiles) == 0 {
		log.Printf("目录 %s 下无Proto文件，跳过处理", protoDir)
		return nil
	}

	// 3. 确保目录存在
	if err := ensureDirExist(config.GrpcTempDirectory); err != nil {
		return fmt.Errorf("创建GRPC临时目录失败: %w", err)
	}
	if err := ensureDirExist(config.GrpcOutputDirectory); err != nil {
		return fmt.Errorf("创建GRPC输出目录失败: %w", err)
	}

	// 4. 生成并拷贝GRPC代码
	if err := generateCppGrpcCode(sourceProtoFiles); err != nil {
		return fmt.Errorf("生成C++ GRPC代码失败: %w", err)
	}
	if err := copyCppGrpcGeneratedFiles(sourceProtoFiles); err != nil {
		return fmt.Errorf("拷贝C++ GRPC代码失败: %w", err)
	}

	return nil
}

// generateCppGrpcCode 调用protoc生成C++ GRPC服务代码（区分操作系统插件）
func generateCppGrpcCode(sourceProtoFiles []string) error {
	// 选择GRPC插件（Linux用无后缀，Windows用.exe）
	grpcPlugin := "grpc_cpp_plugin"
	if runtime.GOOS != "linux" {
		grpcPlugin += ".exe"
	}

	// 将所有路径转换为绝对路径
	absGrpcTempDir, err := filepath.Abs(config.GrpcTempDirectory)
	if err != nil {
		return fmt.Errorf("获取GRPC临时目录绝对路径失败: %w", err)
	}

	absProtoParentDir, err := filepath.Abs(config.ProtoParentIncludePathDir)
	if err != nil {
		return fmt.Errorf("获取ProtoParentIncludePathDir绝对路径失败: %w", err)
	}

	absProtoBufferDir, err := filepath.Abs(config.ProtoBufferDirectory)
	if err != nil {
		return fmt.Errorf("获取ProtoBufferDirectory绝对路径失败: %w", err)
	}

	// 构建protoc参数（全部使用绝对路径）
	args := []string{
		fmt.Sprintf("--grpc_out=%s", absGrpcTempDir),
		fmt.Sprintf("--plugin=protoc-gen-grpc=%s", grpcPlugin),
		fmt.Sprintf("--proto_path=%s", absProtoParentDir),
		fmt.Sprintf("--proto_path=%s", absProtoBufferDir),
	}

	// 确保源文件路径都是绝对路径（补充校验）
	for _, file := range sourceProtoFiles {
		if !filepath.IsAbs(file) {
			absFile, err := filepath.Abs(file)
			if err != nil {
				log.Printf("文件 %s 不是绝对路径且转换失败，跳过: %v", file, err)
				continue
			}
			args = append(args, absFile)
		} else {
			args = append(args, file)
		}
	}

	return execProtocCommand(args, "生成C++ GRPC服务代码")
}

// copyCppGrpcGeneratedFiles 拷贝C++ GRPC生成文件到目标目录
func copyCppGrpcGeneratedFiles(sourceProtoFiles []string) error {
	// 先将所有配置目录转换为绝对路径（只转换一次，提高效率）
	absProtoDir, err := filepath.Abs(config.ProtoDir)
	if err != nil {
		return fmt.Errorf("获取ProtoDir绝对路径失败: %w", err)
	}
	// 统一为斜杠路径用于字符串替换
	absProtoDirSlash := filepath.ToSlash(absProtoDir)

	absGrpcTempDir, err := filepath.Abs(config.GrpcTempDirectory)
	if err != nil {
		return fmt.Errorf("获取GrpcTempDirectory绝对路径失败: %w", err)
	}
	absGrpcTempWithProtoDir := filepath.ToSlash(filepath.Join(absGrpcTempDir, config.ProtoDirName))

	absGrpcOutputDir, err := filepath.Abs(config.GrpcProtoOutputDirectory)
	if err != nil {
		return fmt.Errorf("获取GrpcProtoOutputDirectory绝对路径失败: %w", err)
	}
	absGrpcOutputDirSlash := filepath.ToSlash(absGrpcOutputDir)

	for _, protoFile := range sourceProtoFiles {
		// 确保源文件路径是绝对路径
		if !filepath.IsAbs(protoFile) {
			absProtoFile, err := filepath.Abs(protoFile)
			if err != nil {
				log.Printf("文件 %s 不是绝对路径且转换失败，跳过: %v", protoFile, err)
				continue
			}
			protoFile = absProtoFile
		}
		protoFileSlash := filepath.ToSlash(protoFile)

		// 构建临时文件路径（使用绝对路径替换）
		tempGrpcCppPath := strings.Replace(
			protoFileSlash,
			absProtoDirSlash, // 使用绝对路径替换
			absGrpcTempWithProtoDir,
			1,
		)
		tempGrpcCppPath = strings.Replace(tempGrpcCppPath, config.ProtoExt, config.GrpcPbcEx, 1)
		tempGrpcHeaderPath := strings.Replace(tempGrpcCppPath, config.GrpcPbcEx, config.GrpcPbhEx, 1)

		// 构建目标文件路径（使用绝对路径替换）
		destGrpcCppPath := strings.Replace(
			protoFileSlash,
			absProtoDirSlash, // 使用绝对路径替换
			absGrpcOutputDirSlash,
			1,
		)
		destGrpcCppPath = strings.Replace(destGrpcCppPath, config.ProtoExt, config.GrpcPbcEx, 1)
		destGrpcHeaderPath := strings.Replace(destGrpcCppPath, config.GrpcPbcEx, config.GrpcPbhEx, 1)

		// 转换为系统原生路径
		tempGrpcCppPathNative := filepath.FromSlash(tempGrpcCppPath)
		tempGrpcHeaderPathNative := filepath.FromSlash(tempGrpcHeaderPath)
		destGrpcCppPathNative := filepath.FromSlash(destGrpcCppPath)
		destGrpcHeaderPathNative := filepath.FromSlash(destGrpcHeaderPath)

		// 确保目标目录存在
		if err := ensureDirExist(filepath.Dir(destGrpcCppPathNative)); err != nil {
			log.Printf("创建GRPC目标目录失败 %s: %v", filepath.Dir(destGrpcCppPathNative), err)
			continue // 跳过当前文件，继续处理下一个
		}

		// 拷贝文件（使用log.Printf替代log.Fatalf，避免单个文件失败导致整个程序退出）
		if err := CopyFileIfChanged(tempGrpcCppPathNative, destGrpcCppPathNative); err != nil {
			log.Printf("拷贝GRPC C++文件失败 %s: %v", protoFile, err)
			continue
		}
		if err := CopyFileIfChanged(tempGrpcHeaderPathNative, destGrpcHeaderPathNative); err != nil {
			log.Printf("拷贝GRPC头文件失败 %s: %v", protoFile, err)
			continue
		}
	}
	return nil
}

// ------------------------------
// Go 代码生成相关
// ------------------------------

// GenerateGoGRPCFromProto 递归处理目录下Proto文件，生成Go GRPC代码（跳过数据库Proto）
// collectGoGrpcProtoFiles 递归收集目录下所有非数据库Proto文件（用于Go GRPC代码生成）
func collectGoGrpcProtoFiles(rootDir string) ([]string, error) {
	var protoFiles []string
	err := filepath.WalkDir(rootDir, func(path string, d os.DirEntry, err error) error {
		if err != nil {
			return fmt.Errorf("访问路径 %s 失败: %w", path, err)
		}

		// 跳过目录、非Proto文件和数据库专用Proto文件
		if d.IsDir() || !util.IsProtoFile(d) || d.Name() == config.DbProtoFileName {
			return nil
		}

		// 收集符合条件的Proto文件（转换为统一路径格式）
		absPath, err := filepath.Abs(path)
		if err != nil {
			log.Printf("获取文件 %s 绝对路径失败: %v，跳过该文件", path, err)
			return nil
		}
		protoFiles = append(protoFiles, filepath.ToSlash(absPath))
		return nil
	})

	return protoFiles, err
}

func GenerateGoGRPCFromProto(rootDir string) error {
	// 跳过Etcd服务相关目录
	if util.CheckEtcdServiceExistence(rootDir) {
		return nil
	}

	// 1. 收集非数据库Proto文件
	sourceProtoFiles, err := collectGoGrpcProtoFiles(rootDir)
	if err != nil {
		return fmt.Errorf("收集Go GRPC Proto文件失败: %w", err)
	}
	if len(sourceProtoFiles) == 0 {
		log.Printf("目录 %s 下无需要处理的Proto文件，跳过", rootDir)
		return nil
	}

	// 2. 生成Go GRPC代码
	protoRootPath := filepath.Dir(rootDir) // Proto根路径为当前目录的父目录
	if err := ensureDirExist(config.NodeGoDirectory); err != nil {
		return fmt.Errorf("创建Go节点输出目录失败: %w", err)
	}
	if err := generateGoGrpcCode(sourceProtoFiles, config.NodeGoDirectory, protoRootPath); err != nil {
		return fmt.Errorf("生成Go GRPC代码失败: %w", err)
	}

	// 3. 确保机器人代码目录存在
	return ensureDirExist(config.RobotGoOutputDirectory)
}

// Function to generate C++ files using protoc
func generateCppFiles(sourceProtoFiles []string, outputDir string) error {
	var cmd *exec.Cmd

	args := []string{
		"--cpp_out=" + outputDir,
	}
	args = append(args, sourceProtoFiles...) // 多个 .proto 文件一起处理
	args = append(args,
		"--proto_path="+config.ProtoParentIncludePathDir,
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
func GenerateGoGRPCFromProto1(rootDir string) error {
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
