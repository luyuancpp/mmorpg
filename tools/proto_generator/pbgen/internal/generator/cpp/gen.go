package cpp

import (
	"fmt"
	"log"
	"path/filepath"
	"pbgen/global_value"
	"pbgen/internal"
	_config "pbgen/internal/config"
	_proto "pbgen/internal/prototools"
	utils2 "pbgen/internal/utils"
	"runtime"
	"strings"
	"sync"
)

// BuildProtocCpp 并发处理所有目录的C++代码生成
func BuildProtocCpp(wg *sync.WaitGroup) {
	for i := 0; i < len(global_value.ProtoDirs); i++ {
		wg.Add(1)
		go func(dirIndex int) {
			defer wg.Done()
			dir := global_value.ProtoDirs[dirIndex]
			if err := BuildProtoCpp(wg, dir); err != nil {
				log.Printf("C++批量构建: 目录[%s]处理失败: %v", dir, err)
			}
		}(i)

		wg.Add(1)
		go func(dirIndex int) {
			defer wg.Done()
			dir := global_value.ProtoDirs[dirIndex]
			if err := BuildProtoGrpcCpp(wg, dir); err != nil {
				log.Printf("GRPC C++批量构建: 目录[%s]处理失败: %v", dir, err)
			}
		}(i)
	}
}

// BuildProtoCpp 批量生成指定目录下Proto文件的C++序列化代码
func BuildProtoCpp(wg *sync.WaitGroup, protoDir string) error {
	// 1. 收集Proto文件
	protoFiles, err := utils2.CollectProtoFiles(protoDir)
	if err != nil {
		log.Fatalf("C++批量生成: 收集Proto失败: %w", err)
	}
	if len(protoFiles) == 0 {
		log.Printf("C++批量生成: 目录[%s]无Proto文件，跳过", protoDir)
		return nil
	}

	// 2. 解析目录路径
	cppOutputDir := _config.Global.Paths.ProtoBufCProtoOutputDir
	if err := utils2.EnsureDir(cppOutputDir); err != nil {
		log.Fatalf("C++批量生成: 创建输出目录失败: %w", err)
	}

	cppTempDir := _config.Global.Paths.ProtoBufCTempDir

	// 3. 生成并拷贝C++代码
	if err := GenerateCpp(protoFiles, cppTempDir); err != nil {
		log.Fatalf("C++批量生成: 代码生成失败: %w", err)
	}
	if err := CopyCppOutputs(wg, protoFiles, cppTempDir, cppOutputDir); err != nil {
		log.Fatalf("C++批量生成: 代码拷贝失败: %w", err)
	}

	return nil
}

// GenerateCpp 调用protoc生成C++序列化代码
func GenerateCpp(protoFiles []string, outputDir string) error {
	if err := utils2.EnsureDir(outputDir); err != nil {
		log.Fatalf("C++生成: 创建输出目录失败: %w", err)
	}

	// 解析所有必要路径
	cppOutputDir := outputDir
	protoRootDir := _config.Global.Paths.OutputRoot
	protoBufferDir := _config.Global.Paths.ProtobufDir

	// 构建protoc参数
	args := []string{
		fmt.Sprintf("--cpp_out=%s", cppOutputDir),
		fmt.Sprintf("--proto_path=%s", protoRootDir),
		fmt.Sprintf("--proto_path=%s", protoBufferDir),
	}

	// 补充Proto文件路径（确保绝对路径）
	for _, file := range protoFiles {
		args = append(args, file)
	}

	return utils2.RunProtoc(args, "生成C++序列化代码")
}

// CopyCppOutputs 将临时目录的C++生成文件拷贝到目标目录
func CopyCppOutputs(wg *sync.WaitGroup, protoFiles []string, tempDir, destDir string) error {
	// 解析临时目录和目标目录
	cppGenTempDir := tempDir
	cppDestDir := destDir
	// 解析Proto根目录（用于计算相对路径）
	protoRootDir := _config.Global.Paths.OutputRoot

	for _, protoFile := range protoFiles {
		// 计算Proto文件相对根目录的路径（保持目录结构）
		protoRelPath, err := filepath.Rel(protoRootDir, protoFile)
		if err != nil {
			log.Printf("C++拷贝: 计算[%s]相对路径失败: %v，跳过", protoFile, err)
			continue
		}
		protoRelDir := filepath.Dir(protoRelPath)

		// 构建生成文件名（.proto → .pb.h/.pb.cc）
		protoFileName := filepath.Base(protoFile)
		headerFile := strings.Replace(protoFileName, _config.Global.FileExtensions.Proto, _config.Global.FileExtensions.PbH, 1)
		cppFile := strings.Replace(protoFileName, _config.Global.FileExtensions.Proto, _config.Global.FileExtensions.PbCc, 1)

		// 构建临时文件和目标文件路径
		tempHeaderPath := filepath.Join(cppGenTempDir, protoRelDir, headerFile)
		tempCppPath := filepath.Join(cppGenTempDir, protoRelDir, cppFile)
		destHeaderPath := filepath.Join(cppDestDir, protoRelDir, headerFile)
		destCppPath := filepath.Join(cppDestDir, protoRelDir, cppFile)

		// 确保目标目录存在
		if err := utils2.EnsureDir(filepath.Dir(destHeaderPath)); err != nil {
			log.Printf("C++拷贝: 创建目标目录[%s]失败: %v，跳过", filepath.Dir(destHeaderPath), err)
			continue
		}

		// 拷贝文件
		utils2.CopyFileIfChangedAsync(wg, tempHeaderPath, destHeaderPath)
		utils2.CopyFileIfChangedAsync(wg, tempCppPath, destCppPath)
	}
	return nil
}

// BuildProtoGrpcCpp 生成指定目录下Proto文件的C++ GRPC服务代码
func BuildProtoGrpcCpp(wg *sync.WaitGroup, protoDir string) error {
	// 1. 检查是否包含GRPC服务定义
	if !utils2.HasGrpcService(strings.ToLower(protoDir)) {
		log.Printf("GRPC C++生成: 目录[%s]无GRPC服务定义，跳过", protoDir)
		return nil
	}

	// 2. 收集Proto文件
	protoFiles, err := utils2.CollectProtoFiles(protoDir)
	if err != nil {
		log.Fatalf("GRPC C++生成: 收集Proto失败: %w", err)
	}
	if len(protoFiles) == 0 {
		log.Printf("GRPC C++生成: 目录[%s]无Proto文件，跳过", protoDir)
		return nil
	}

	// 3. 确保目录存在
	if err := utils2.EnsureDir(_config.Global.Paths.GrpcTempDir); err != nil {
		log.Fatalf("GRPC C++生成: 创建临时目录失败: %w", err)
	}
	if err := utils2.EnsureDir(_config.Global.Paths.GrpcOutputDir); err != nil {
		log.Fatalf("GRPC C++生成: 创建输出目录失败: %w", err)
	}

	// 4. 生成并拷贝GRPC代码
	if err := GenerateCppGrpc(protoFiles); err != nil {
		log.Fatalf("GRPC C++生成: 代码生成失败: %w", err)
	}
	if err := copyCppGrpcOutputs(wg, protoFiles); err != nil {
		log.Fatalf("GRPC C++生成: 代码拷贝失败: %w", err)
	}

	return nil
}

// GenerateCppGrpc 调用protoc生成C++ GRPC服务代码（区分操作系统插件）
func GenerateCppGrpc(protoFiles []string) error {
	// 选择GRPC插件（Linux用无后缀，Windows用.exe）
	grpcPlugin := "grpc_cpp_plugin"
	if runtime.GOOS != "linux" {
		grpcPlugin += ".exe"
	}

	// 将所有路径转换为绝对路径
	grpcTempDir := _config.Global.Paths.GrpcTempDir

	protoParentDir := _config.Global.Paths.OutputRoot

	protoBufferDir := _config.Global.Paths.ProtobufDir

	// 构建protoc参数
	args := []string{
		fmt.Sprintf("--grpc_out=%s", grpcTempDir),
		fmt.Sprintf("--plugin=protoc-gen-grpc=%s", grpcPlugin),
		fmt.Sprintf("--proto_path=%s", protoParentDir),
		fmt.Sprintf("--proto_path=%s", protoBufferDir),
	}

	// 补充Proto文件路径
	for _, file := range protoFiles {
		args = append(args, file)
	}

	return utils2.RunProtoc(args, "生成C++ GRPC服务代码")
}

// copyCppGrpcOutputs 拷贝C++ GRPC生成文件到目标目录
func copyCppGrpcOutputs(wg *sync.WaitGroup, protoFiles []string) error {
	// 解析所有必要路径
	protoDir := _config.Global.Paths.ProtoDir
	protoDirSlash := filepath.ToSlash(protoDir)

	grpcTempWithProtoDir := filepath.ToSlash(_config.Global.Paths.GrpcTempDir + _config.Global.DirectoryNames.ProtoDirName)

	grpcOutputDir := _config.Global.Paths.GrpcProtoOutputDir

	grpcOutputDirSlash := filepath.ToSlash(grpcOutputDir)

	for _, protoFile := range protoFiles {
		// 确保Proto文件是绝对路径
		protoFileSlash := filepath.ToSlash(protoFile)

		// 构建临时文件路径
		tempGrpcCppPath := strings.Replace(
			protoFileSlash,
			protoDirSlash,
			grpcTempWithProtoDir,
			1,
		)
		tempGrpcCppPath = strings.Replace(tempGrpcCppPath, _config.Global.FileExtensions.Proto, _config.Global.FileExtensions.GrpcPbCc, 1)
		tempGrpcHeaderPath := strings.Replace(tempGrpcCppPath, _config.Global.FileExtensions.GrpcPbCc, _config.Global.FileExtensions.GrpcPbH, 1)

		// 构建目标文件路径
		destGrpcCppPath := strings.Replace(
			protoFileSlash,
			protoDirSlash,
			grpcOutputDirSlash,
			1,
		)
		destGrpcCppPath = strings.Replace(destGrpcCppPath, _config.Global.FileExtensions.Proto, _config.Global.FileExtensions.GrpcPbCc, 1)
		destGrpcHeaderPath := strings.Replace(destGrpcCppPath, _config.Global.FileExtensions.GrpcPbCc, _config.Global.FileExtensions.GrpcPbH, 1)

		// 转换为系统原生路径
		tempGrpcCppPathNative := filepath.FromSlash(tempGrpcCppPath)
		tempGrpcHeaderPathNative := filepath.FromSlash(tempGrpcHeaderPath)
		destGrpcCppPathNative := filepath.FromSlash(destGrpcCppPath)
		destGrpcHeaderPathNative := filepath.FromSlash(destGrpcHeaderPath)

		// 确保目标目录存在
		if err := utils2.EnsureDir(filepath.Dir(destGrpcCppPathNative)); err != nil {
			log.Printf("GRPC C++拷贝: 创建目录[%s]失败: %v，跳过", filepath.Dir(destGrpcCppPathNative), err)
			continue
		}

		// 拷贝文件
		utils2.CopyFileIfChangedAsync(wg, tempGrpcCppPathNative, destGrpcCppPathNative)
		utils2.CopyFileIfChangedAsync(wg, tempGrpcHeaderPathNative, destGrpcHeaderPathNative)
	}
	return nil
}

// generateGameGrpcCpp 生成游戏GRPC C++代码
func generateGameGrpcCpp(wg *sync.WaitGroup, protoFiles []string) error {
	wg.Add(1)
	go func() {
		defer wg.Done()

		// 解析输出目录
		cppOutputDir := _config.Global.Paths.ProtoBufCProtoOutputDir
		if err := utils2.EnsureDir(cppOutputDir); err != nil {
			log.Fatalf("创建C++输出目录失败: %w", err)
		}

		// 解析临时目录
		cppTempDir := _config.Global.Paths.ProtoBufCTempDir

		// 生成C++代码
		if err := GenerateCpp(protoFiles, cppTempDir); err != nil {
			log.Fatalf("生成C++代码失败: %w", err)
		}

		// 拷贝C++代码到目标目录
		cppDestDir := _config.Global.Paths.GrpcOutputDir

		if err := CopyCppOutputs(wg, protoFiles, cppTempDir, cppDestDir); err != nil {
			log.Fatalf("拷贝C++代码失败: %w", err)
		}

	}()

	return nil
}

// generateGameGrpcImpl 游戏GRPC生成核心逻辑
func generateGameGrpcImpl(wg *sync.WaitGroup) error {
	// 1. 解析游戏Proto文件路径
	gameProtoPath, err := _proto.ResolveGameProtoPath()
	if err != nil {
		log.Fatalf("解析Proto路径失败: %w", err)
	}
	protoFiles := []string{gameProtoPath}

	// 2. 生成C++序列化代码
	if err := generateGameGrpcCpp(wg, protoFiles); err != nil {
		log.Fatalf("C++代码生成失败: %w", err)
	}

	return nil
}

// GenerateGameGrpc 生成游戏GRPC代码（C++序列化+Go节点代码）
func GenerateGameGrpc(wg *sync.WaitGroup) {
	wg.Add(1)
	go func() {
		wg.Done()
		if err := generateGameGrpcImpl(wg); err != nil {
			log.Printf("游戏GRPC生成: 整体失败: %v", err)
		}
	}()
}

func GeneratorHandler(wg *sync.WaitGroup) {
	for _, service := range internal.GlobalRPCServiceList {
		ProcessAllHandlers(wg, service.MethodInfo)
	}
}

func WriteMethodFile(wg *sync.WaitGroup) {
	// Concurrent operations for game, centre, and gate registers
	wg.Add(1)
	go internal.GenRegisterFile(wg, _config.Global.PathLists.MethodHandlerDirectories.RoomNode+_config.Global.FileExtensions.RegisterHandlerCppExtension, IsRoomNodeHostedProtocolHandler)
	wg.Add(1)
	go internal.WriteRepliedRegisterFile(wg, _config.Global.PathLists.MethodHandlerDirectories.RoomNodeReplied+_config.Global.FileExtensions.RegisterRepliedHandlerCppExtension, IsRoomNodeReceivedProtocolResponseHandler)

	wg.Add(1)
	go internal.GenRegisterFile(wg, _config.Global.PathLists.MethodHandlerDirectories.CentreNode+_config.Global.FileExtensions.RegisterHandlerCppExtension, IsCentreHostedServiceHandler)
	wg.Add(1)
	go internal.WriteRepliedRegisterFile(wg, _config.Global.PathLists.MethodHandlerDirectories.CentreNodeReplied+_config.Global.FileExtensions.RegisterRepliedHandlerCppExtension, IsCentreReceivedServiceResponseHandler)

	wg.Add(1)
	go internal.GenRegisterFile(wg, _config.Global.PathLists.MethodHandlerDirectories.GateNodeReplied+_config.Global.FileExtensions.RegisterHandlerCppExtension, IsNoOpHandler)
	wg.Add(1)
	go internal.WriteRepliedRegisterFile(wg, _config.Global.PathLists.MethodHandlerDirectories.GateNodeReplied+_config.Global.FileExtensions.RegisterRepliedHandlerCppExtension, IsGateNodeReceivedResponseHandler)
}
