package cpp

import (
	"fmt"
	"log"
	"path/filepath"
	"pbgen/config"
	"pbgen/internal"
	_config "pbgen/internal/config"
	"pbgen/internal/proto"
	utils2 "pbgen/internal/utils"
	"runtime"
	"strings"
)

// BuildProtocCpp 并发处理所有目录的C++代码生成
func BuildProtocCpp() {
	for i := 0; i < len(config.ProtoDirs); i++ {
		utils2.Wg.Add(1)
		go func(dirIndex int) {
			defer utils2.Wg.Done()
			dir := config.ProtoDirs[dirIndex]
			if err := BuildProtoCpp(dir); err != nil {
				log.Printf("C++批量构建: 目录[%s]处理失败: %v", dir, err)
			}
		}(i)

		utils2.Wg.Add(1)
		go func(dirIndex int) {
			defer utils2.Wg.Done()
			dir := config.ProtoDirs[dirIndex]
			if err := BuildProtoGrpcCpp(dir); err != nil {
				log.Printf("GRPC C++批量构建: 目录[%s]处理失败: %v", dir, err)
			}
		}(i)
	}
}

// BuildProtoCpp 批量生成指定目录下Proto文件的C++序列化代码
func BuildProtoCpp(protoDir string) error {
	// 1. 收集Proto文件
	protoFiles, err := utils2.CollectProtoFiles(protoDir)
	if err != nil {
		return fmt.Errorf("C++批量生成: 收集Proto失败: %w", err)
	}
	if len(protoFiles) == 0 {
		log.Printf("C++批量生成: 目录[%s]无Proto文件，跳过", protoDir)
		return nil
	}

	// 2. 解析目录路径
	cppOutputDir, err := utils2.ResolveAbsPath(config.PbcProtoOutputDirectory, "C++批量输出目录")
	if err != nil {
		return err
	}
	if err := utils2.EnsureDir(cppOutputDir); err != nil {
		return fmt.Errorf("C++批量生成: 创建输出目录失败: %w", err)
	}

	cppTempDir, err := utils2.ResolveAbsPath(config.PbcTempDirectory, "C++批量临时目录")
	if err != nil {
		return err
	}

	// 3. 生成并拷贝C++代码
	if err := GenerateCpp(protoFiles, cppTempDir); err != nil {
		return fmt.Errorf("C++批量生成: 代码生成失败: %w", err)
	}
	if err := CopyCppOutputs(protoFiles, cppTempDir, cppOutputDir); err != nil {
		return fmt.Errorf("C++批量生成: 代码拷贝失败: %w", err)
	}

	return nil
}

// GenerateCpp 调用protoc生成C++序列化代码
func GenerateCpp(protoFiles []string, outputDir string) error {
	if err := utils2.EnsureDir(outputDir); err != nil {
		return fmt.Errorf("C++生成: 创建输出目录失败: %w", err)
	}

	// 解析所有必要路径
	cppOutputDir, err := utils2.ResolveAbsPath(outputDir, "C++生成输出目录")
	if err != nil {
		return err
	}

	protoRootDir, err := utils2.ResolveAbsPath(_config.Global.Paths.OutputRoot, "Proto根目录")
	if err != nil {
		return err
	}

	protoBufferDir, err := utils2.ResolveAbsPath(config.ProtoBufferDirectory, "ProtoBuffer目录")
	if err != nil {
		return err
	}

	// 构建protoc参数
	args := []string{
		fmt.Sprintf("--cpp_out=%s", cppOutputDir),
		fmt.Sprintf("--proto_path=%s", protoRootDir),
		fmt.Sprintf("--proto_path=%s", protoBufferDir),
	}

	// 补充Proto文件路径（确保绝对路径）
	for _, file := range protoFiles {
		absFile, err := utils2.ResolveAbsPath(file, "Proto源文件")
		if err != nil {
			log.Printf("C++生成: 跳过无效文件[%s]: %v", file, err)
			continue
		}
		args = append(args, absFile)
	}

	return utils2.RunProtoc(args, "生成C++序列化代码")
}

// CopyCppOutputs 将临时目录的C++生成文件拷贝到目标目录
func CopyCppOutputs(protoFiles []string, tempDir, destDir string) error {
	// 解析临时目录和目标目录
	cppTempDir, err := utils2.ResolveAbsPath(tempDir, "C++拷贝临时目录")
	if err != nil {
		return err
	}

	cppDestDir, err := utils2.ResolveAbsPath(destDir, "C++拷贝目标目录")
	if err != nil {
		return err
	}

	// 解析Proto根目录（用于计算相对路径）
	protoRootDir, err := utils2.ResolveAbsPath(_config.Global.Paths.OutputRoot, "Proto根目录")
	if err != nil {
		return err
	}

	for _, protoFile := range protoFiles {
		// 确保Proto文件是绝对路径
		absProtoFile, err := utils2.ResolveAbsPath(protoFile, "Proto源文件")
		if err != nil {
			log.Printf("C++拷贝: 跳过无效文件[%s]: %v", protoFile, err)
			continue
		}

		// 计算Proto文件相对根目录的路径（保持目录结构）
		protoRelPath, err := filepath.Rel(protoRootDir, absProtoFile)
		if err != nil {
			log.Printf("C++拷贝: 计算[%s]相对路径失败: %v，跳过", absProtoFile, err)
			continue
		}
		protoRelDir := filepath.Dir(protoRelPath)

		// 构建生成文件名（.proto → .pb.h/.pb.cc）
		protoFileName := filepath.Base(absProtoFile)
		headerFile := strings.Replace(protoFileName, config.ProtoExt, config.ProtoPbhEx, 1)
		cppFile := strings.Replace(protoFileName, config.ProtoExt, config.ProtoPbcEx, 1)

		// 构建临时文件和目标文件路径
		tempHeaderPath := filepath.Join(cppTempDir, protoRelDir, headerFile)
		tempCppPath := filepath.Join(cppTempDir, protoRelDir, cppFile)
		destHeaderPath := filepath.Join(cppDestDir, protoRelDir, headerFile)
		destCppPath := filepath.Join(cppDestDir, protoRelDir, cppFile)

		// 确保目标目录存在
		if err := utils2.EnsureDir(filepath.Dir(destHeaderPath)); err != nil {
			log.Printf("C++拷贝: 创建目标目录[%s]失败: %v，跳过", filepath.Dir(destHeaderPath), err)
			continue
		}

		// 拷贝文件
		if err := utils2.CopyFileIfChanged(tempHeaderPath, destHeaderPath); err != nil {
			log.Printf("C++拷贝: 头文件[%s]失败: %v，跳过", protoFile, err)
			continue
		}
		if err := utils2.CopyFileIfChanged(tempCppPath, destCppPath); err != nil {
			log.Printf("C++拷贝: 实现文件[%s]失败: %v，跳过", protoFile, err)
			continue
		}
	}
	return nil
}

// BuildProtoGrpcCpp 生成指定目录下Proto文件的C++ GRPC服务代码
func BuildProtoGrpcCpp(protoDir string) error {
	// 1. 检查是否包含GRPC服务定义
	if !utils2.HasGrpcService(strings.ToLower(protoDir)) {
		log.Printf("GRPC C++生成: 目录[%s]无GRPC服务定义，跳过", protoDir)
		return nil
	}

	// 2. 收集Proto文件
	protoFiles, err := utils2.CollectProtoFiles(protoDir)
	if err != nil {
		return fmt.Errorf("GRPC C++生成: 收集Proto失败: %w", err)
	}
	if len(protoFiles) == 0 {
		log.Printf("GRPC C++生成: 目录[%s]无Proto文件，跳过", protoDir)
		return nil
	}

	// 3. 确保目录存在
	if err := utils2.EnsureDir(config.GrpcTempDirectory); err != nil {
		return fmt.Errorf("GRPC C++生成: 创建临时目录失败: %w", err)
	}
	if err := utils2.EnsureDir(config.GrpcOutputDirectory); err != nil {
		return fmt.Errorf("GRPC C++生成: 创建输出目录失败: %w", err)
	}

	// 4. 生成并拷贝GRPC代码
	if err := GenerateCppGrpc(protoFiles); err != nil {
		return fmt.Errorf("GRPC C++生成: 代码生成失败: %w", err)
	}
	if err := copyCppGrpcOutputs(protoFiles); err != nil {
		return fmt.Errorf("GRPC C++生成: 代码拷贝失败: %w", err)
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
	grpcTempDir, err := utils2.ResolveAbsPath(config.GrpcTempDirectory, "GRPC临时目录")
	if err != nil {
		return fmt.Errorf("GRPC C++生成: 解析临时目录失败: %w", err)
	}

	protoParentDir, err := utils2.ResolveAbsPath(config.ProtoParentIncludePathDir, "Proto父目录")
	if err != nil {
		return fmt.Errorf("GRPC C++生成: 解析父目录失败: %w", err)
	}

	protoBufferDir, err := utils2.ResolveAbsPath(config.ProtoBufferDirectory, "ProtoBuffer目录")
	if err != nil {
		return fmt.Errorf("GRPC C++生成: 解析ProtoBuffer目录失败: %w", err)
	}

	// 构建protoc参数
	args := []string{
		fmt.Sprintf("--grpc_out=%s", grpcTempDir),
		fmt.Sprintf("--plugin=protoc-gen-grpc=%s", grpcPlugin),
		fmt.Sprintf("--proto_path=%s", protoParentDir),
		fmt.Sprintf("--proto_path=%s", protoBufferDir),
	}

	// 补充Proto文件路径
	for _, file := range protoFiles {
		absFile, err := utils2.ResolveAbsPath(file, "GRPC Proto源文件")
		if err != nil {
			log.Printf("GRPC C++生成: 跳过无效文件[%s]: %v", file, err)
			continue
		}
		args = append(args, absFile)
	}

	return utils2.RunProtoc(args, "生成C++ GRPC服务代码")
}

// copyCppGrpcOutputs 拷贝C++ GRPC生成文件到目标目录
func copyCppGrpcOutputs(protoFiles []string) error {
	// 解析所有必要路径
	protoDir, err := utils2.ResolveAbsPath(_config.Global.Paths.ProtoDir, "Proto基础目录")
	if err != nil {
		return err
	}
	protoDirSlash := filepath.ToSlash(protoDir)

	grpcTempDir, err := utils2.ResolveAbsPath(config.GrpcTempDirectory, "GRPC临时目录")
	if err != nil {
		return err
	}
	grpcTempWithProtoDir := filepath.ToSlash(filepath.Join(grpcTempDir, config.ProtoDirName))

	grpcOutputDir, err := utils2.ResolveAbsPath(config.GrpcProtoOutputDirectory, "GRPC输出目录")
	if err != nil {
		return err
	}
	grpcOutputDirSlash := filepath.ToSlash(grpcOutputDir)

	for _, protoFile := range protoFiles {
		// 确保Proto文件是绝对路径
		absProtoFile, err := utils2.ResolveAbsPath(protoFile, "GRPC Proto源文件")
		if err != nil {
			log.Printf("GRPC C++拷贝: 跳过无效文件[%s]: %v", protoFile, err)
			continue
		}
		protoFileSlash := filepath.ToSlash(absProtoFile)

		// 构建临时文件路径
		tempGrpcCppPath := strings.Replace(
			protoFileSlash,
			protoDirSlash,
			grpcTempWithProtoDir,
			1,
		)
		tempGrpcCppPath = strings.Replace(tempGrpcCppPath, config.ProtoExt, config.GrpcPbcEx, 1)
		tempGrpcHeaderPath := strings.Replace(tempGrpcCppPath, config.GrpcPbcEx, config.GrpcPbhEx, 1)

		// 构建目标文件路径
		destGrpcCppPath := strings.Replace(
			protoFileSlash,
			protoDirSlash,
			grpcOutputDirSlash,
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
		if err := utils2.EnsureDir(filepath.Dir(destGrpcCppPathNative)); err != nil {
			log.Printf("GRPC C++拷贝: 创建目录[%s]失败: %v，跳过", filepath.Dir(destGrpcCppPathNative), err)
			continue
		}

		// 拷贝文件
		if err := utils2.CopyFileIfChanged(tempGrpcCppPathNative, destGrpcCppPathNative); err != nil {
			log.Printf("GRPC C++拷贝: C++文件[%s]失败: %v，跳过", protoFile, err)
			continue
		}
		if err := utils2.CopyFileIfChanged(tempGrpcHeaderPathNative, destGrpcHeaderPathNative); err != nil {
			log.Printf("GRPC C++拷贝: 头文件[%s]失败: %v，跳过", protoFile, err)
			continue
		}
	}
	return nil
}

// generateGameGrpcCpp 生成游戏GRPC C++代码
func generateGameGrpcCpp(protoFiles []string) error {
	// 解析输出目录
	cppOutputDir, err := utils2.ResolveAbsPath(config.PbcProtoOutputDirectory, "游戏C++输出目录")
	if err != nil {
		return err
	}
	if err := utils2.EnsureDir(cppOutputDir); err != nil {
		return fmt.Errorf("创建C++输出目录失败: %w", err)
	}

	// 解析临时目录
	cppTempDir, err := utils2.ResolveAbsPath(config.PbcTempDirectory, "游戏C++临时目录")
	if err != nil {
		return err
	}

	// 生成C++代码
	if err := GenerateCpp(protoFiles, cppTempDir); err != nil {
		return fmt.Errorf("生成C++代码失败: %w", err)
	}

	// 拷贝C++代码到目标目录
	cppDestDir, err := utils2.ResolveAbsPath(config.PbcProtoOutputNoProtoSuffixPath, "游戏C++最终目录")
	if err != nil {
		return err
	}
	if err := CopyCppOutputs(protoFiles, cppTempDir, cppDestDir); err != nil {
		return fmt.Errorf("拷贝C++代码失败: %w", err)
	}

	return nil
}

// generateGameGrpcImpl 游戏GRPC生成核心逻辑
func generateGameGrpcImpl() error {
	// 1. 解析游戏Proto文件路径
	gameProtoPath, err := proto.ResolveGameProtoPath()
	if err != nil {
		return fmt.Errorf("解析Proto路径失败: %w", err)
	}
	protoFiles := []string{gameProtoPath}

	// 2. 生成C++序列化代码
	if err := generateGameGrpcCpp(protoFiles); err != nil {
		return fmt.Errorf("C++代码生成失败: %w", err)
	}

	return nil
}

// GenerateGameGrpc 生成游戏GRPC代码（C++序列化+Go节点代码）
func GenerateGameGrpc() error {
	utils2.Wg.Add(1)
	go func() {
		defer utils2.Wg.Done()
		if err := generateGameGrpcImpl(); err != nil {
			log.Printf("游戏GRPC生成: 整体失败: %v", err)
		}
	}()
	return nil
}

func GeneratorHandler() {
	for _, service := range internal.GlobalRPCServiceList {
		ProcessAllHandlers(service.MethodInfo)
	}
}

func WriteMethodFile() {
	// Concurrent operations for game, centre, and gate registers
	utils2.Wg.Add(1)
	go internal.GenRegisterFile(config.RoomNodeMethodHandlerDirectory+config.RegisterHandlerCppExtension, IsRoomNodeHostedProtocolHandler)
	utils2.Wg.Add(1)
	go internal.WriteRepliedRegisterFile(config.RoomNodeMethodRepliedHandlerDirectory+config.RegisterRepliedHandlerCppExtension, IsRoomNodeReceivedProtocolResponseHandler)

	utils2.Wg.Add(1)
	go internal.GenRegisterFile(config.CentreNodeMethodHandlerDirectory+config.RegisterHandlerCppExtension, IsCentreHostedServiceHandler)
	utils2.Wg.Add(1)
	go internal.WriteRepliedRegisterFile(config.CentreMethodRepliedHandleDir+config.RegisterRepliedHandlerCppExtension, IsCentreReceivedServiceResponseHandler)

	utils2.Wg.Add(1)
	go internal.GenRegisterFile(config.GateMethodRepliedHandlerDirectory+config.RegisterHandlerCppExtension, IsNoOpHandler)
	utils2.Wg.Add(1)
	go internal.WriteRepliedRegisterFile(config.GateMethodRepliedHandlerDirectory+config.RegisterRepliedHandlerCppExtension, IsGateNodeReceivedResponseHandler)
}
