package cpp

import (
	"fmt"
	"os"
	"path/filepath"
	"pbgen/internal"
	_config "pbgen/internal/config"
	_proto "pbgen/internal/prototools"
	utils2 "pbgen/internal/utils"
	"pbgen/logger"
	"runtime"
	"strings"
	"sync"

	"go.uber.org/zap"
)

// BuildProtocCpp 并发处理所有目录的C++代码生成
func BuildProtocCpp(wg *sync.WaitGroup) {
	for _, meta := range _config.Global.DomainMeta {
		wg.Add(1)
		go func(meta _config.DomainMeta) {
			defer wg.Done()
			dir := meta.Source
			if err := BuildProtoCpp(wg, dir); err != nil {
				logger.Global.Warn("C++批量构建失败",
					zap.String("目录", dir),
					zap.Error(err),
				)
			}
		}(meta)

		wg.Add(1)
		go func(meta _config.DomainMeta) {
			defer wg.Done()
			dir := meta.Source
			if err := BuildProtoGrpcCpp(wg, dir); err != nil {
				logger.Global.Warn("GRPC C++批量构建失败",
					zap.String("目录", dir),
					zap.Error(err),
				)
			}
		}(meta)
	}
}

// BuildProtoCpp 批量生成指定目录下Proto文件的C++序列化代码
func BuildProtoCpp(wg *sync.WaitGroup, protoDir string) error {
	if _, statErr := os.Stat(protoDir); statErr != nil {
		if os.IsNotExist(statErr) {
			logger.Global.Warn("C++批量生成: Proto目录不存在，跳过",
				zap.String("目录", protoDir),
			)
			return nil
		}
		return statErr
	}

	protoFiles, err := utils2.CollectProtoFiles(protoDir)
	if err != nil {
		return err
	}
	if len(protoFiles) == 0 {
		logger.Global.Info("C++批量生成: 无Proto文件，跳过",
			zap.String("目录", protoDir),
		)
		return nil
	}

	cppOutputDir := _config.Global.Paths.ProtoBufCProtoOutputDir
	if err := utils2.EnsureDir(cppOutputDir); err != nil {
		return err
	}

	cppTempDir := _config.Global.Paths.ProtoBufCTempDir

	if err := GenerateCpp(protoFiles, cppTempDir); err != nil {
		return err
	}
	if err := CopyCppOutputs(wg, protoFiles, cppTempDir, cppOutputDir); err != nil {
		return err
	}

	return nil
}

// GenerateCpp 调用protoc生成C++序列化代码
func GenerateCpp(protoFiles []string, outputDir string) error {
	if err := utils2.EnsureDir(outputDir); err != nil {
		logger.Global.Fatal("C++生成: 创建输出目录失败",
			zap.Error(err),
		)
	}

	cppOutputDir := outputDir
	protoRootDir := _config.Global.Paths.OutputRoot
	protoBufferDir := _config.Global.Paths.ProtobufDir

	args := []string{
		fmt.Sprintf("--cpp_out=%s", cppOutputDir),
		fmt.Sprintf("--proto_path=%s", protoRootDir),
		fmt.Sprintf("--proto_path=%s", protoBufferDir),
	}

	for _, file := range protoFiles {
		args = append(args, file)
	}

	return utils2.RunProtoc(args, "生成C++序列化代码")
}

// CopyCppOutputs 将临时目录的C++生成文件拷贝到目标目录
func CopyCppOutputs(wg *sync.WaitGroup, protoFiles []string, tempDir, destDir string) error {
	cppGenTempDir := tempDir
	cppDestDir := destDir
	protoRootDir := _config.Global.Paths.OutputRoot

	for _, protoFile := range protoFiles {
		protoRelPath, err := filepath.Rel(protoRootDir, protoFile)
		if err != nil {
			logger.Global.Warn("C++拷贝: 计算相对路径失败，跳过",
				zap.String("Proto文件", protoFile),
				zap.Error(err),
			)
			continue
		}
		protoRelDir := filepath.Dir(protoRelPath)

		protoFileName := filepath.Base(protoFile)
		headerFile := strings.Replace(protoFileName, _config.Global.FileExtensions.Proto, _config.Global.FileExtensions.PbH, 1)
		cppFile := strings.Replace(protoFileName, _config.Global.FileExtensions.Proto, _config.Global.FileExtensions.PbCc, 1)

		tempHeaderPath := filepath.Join(cppGenTempDir, protoRelDir, headerFile)
		tempCppPath := filepath.Join(cppGenTempDir, protoRelDir, cppFile)
		destHeaderPath := filepath.Join(cppDestDir, protoRelDir, headerFile)
		destCppPath := filepath.Join(cppDestDir, protoRelDir, cppFile)

		if err := utils2.EnsureDir(filepath.Dir(destHeaderPath)); err != nil {
			logger.Global.Warn("C++拷贝: 创建目标目录失败，跳过",
				zap.String("目标目录", filepath.Dir(destHeaderPath)),
				zap.Error(err),
			)
			continue
		}

		utils2.CopyFileIfChangedAsync(wg, tempHeaderPath, destHeaderPath)
		utils2.CopyFileIfChangedAsync(wg, tempCppPath, destCppPath)
	}
	return nil
}

// BuildProtoGrpcCpp 生成指定目录下Proto文件的C++ GRPC服务代码
func BuildProtoGrpcCpp(wg *sync.WaitGroup, protoDir string) error {
	if !(utils2.HasGrpcService(strings.ToLower(protoDir)) || utils2.HasEtcdService(protoDir)) {
		logger.Global.Info("GRPC C++生成: 无GRPC服务定义，跳过",
			zap.String("目录", protoDir),
		)
		return nil
	}

	if _, statErr := os.Stat(protoDir); statErr != nil {
		if os.IsNotExist(statErr) {
			logger.Global.Warn("GRPC C++生成: Proto目录不存在，跳过",
				zap.String("目录", protoDir),
			)
			return nil
		}
		return statErr
	}

	protoFiles, err := utils2.CollectProtoFiles(protoDir)
	if err != nil {
		return err
	}
	if len(protoFiles) == 0 {
		logger.Global.Info("GRPC C++生成: 无Proto文件，跳过",
			zap.String("目录", protoDir),
		)
		return nil
	}

	if err := utils2.EnsureDir(_config.Global.Paths.GrpcTempDir); err != nil {
		return err
	}
	if err := utils2.EnsureDir(_config.Global.Paths.GrpcOutputDir); err != nil {
		return err
	}

	if err := GenerateCppGrpc(protoFiles); err != nil {
		return err
	}
	if err := copyCppGrpcOutputs(wg, protoFiles); err != nil {
		return err
	}

	return nil
}

// GenerateCppGrpc 调用protoc生成C++ GRPC服务代码（区分操作系统插件）
func GenerateCppGrpc(protoFiles []string) error {
	grpcPlugin := "grpc_cpp_plugin"
	if runtime.GOOS != "linux" {
		grpcPlugin += ".exe"
	}

	grpcTempDir := _config.Global.Paths.GrpcTempDir

	protoParentDir := _config.Global.Paths.OutputRoot

	protoBufferDir := _config.Global.Paths.ProtobufDir

	args := []string{
		fmt.Sprintf("--grpc_out=%s", grpcTempDir),
		fmt.Sprintf("--plugin=protoc-gen-grpc=%s", grpcPlugin),
		fmt.Sprintf("--proto_path=%s", protoParentDir),
		fmt.Sprintf("--proto_path=%s", protoBufferDir),
	}

	for _, file := range protoFiles {
		args = append(args, file)
	}

	return utils2.RunProtoc(args, "生成C++ GRPC服务代码")
}

// copyCppGrpcOutputs 拷贝C++ GRPC生成文件到目标目录
func copyCppGrpcOutputs(wg *sync.WaitGroup, protoFiles []string) error {
	protoDir := _config.Global.Paths.ProtoDir
	protoDirSlash := filepath.ToSlash(protoDir)

	grpcTempWithProtoDir := filepath.ToSlash(_config.Global.Paths.GrpcTempDir + _config.Global.DirectoryNames.ProtoDirName)

	grpcOutputDir := _config.Global.Paths.GrpcProtoOutputDir

	grpcOutputDirSlash := filepath.ToSlash(grpcOutputDir)

	for _, protoFile := range protoFiles {
		protoFileSlash := filepath.ToSlash(protoFile)

		tempGrpcCppPath := strings.Replace(
			protoFileSlash,
			protoDirSlash,
			grpcTempWithProtoDir,
			1,
		)
		tempGrpcCppPath = strings.Replace(tempGrpcCppPath, _config.Global.FileExtensions.Proto, _config.Global.FileExtensions.GrpcPbCc, 1)
		tempGrpcHeaderPath := strings.Replace(tempGrpcCppPath, _config.Global.FileExtensions.GrpcPbCc, _config.Global.FileExtensions.GrpcPbH, 1)

		destGrpcCppPath := strings.Replace(
			protoFileSlash,
			protoDirSlash,
			grpcOutputDirSlash,
			1,
		)
		destGrpcCppPath = strings.Replace(destGrpcCppPath, _config.Global.FileExtensions.Proto, _config.Global.FileExtensions.GrpcPbCc, 1)
		destGrpcHeaderPath := strings.Replace(destGrpcCppPath, _config.Global.FileExtensions.GrpcPbCc, _config.Global.FileExtensions.GrpcPbH, 1)

		tempGrpcCppPathNative := filepath.FromSlash(tempGrpcCppPath)
		tempGrpcHeaderPathNative := filepath.FromSlash(tempGrpcHeaderPath)
		destGrpcCppPathNative := filepath.FromSlash(destGrpcCppPath)
		destGrpcHeaderPathNative := filepath.FromSlash(destGrpcHeaderPath)

		if err := utils2.EnsureDir(filepath.Dir(destGrpcCppPathNative)); err != nil {
			logger.Global.Warn("GRPC C++拷贝: 创建目录失败，跳过",
				zap.String("目标目录", filepath.Dir(destGrpcCppPathNative)),
				zap.Error(err),
			)
			continue
		}

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

		cppOutputDir := _config.Global.Paths.ProtoBufCProtoOutputDir
		if err := utils2.EnsureDir(cppOutputDir); err != nil {
			logger.Global.Fatal("创建C++输出目录失败",
				zap.Error(err),
			)
		}

		cppTempDir := _config.Global.Paths.ProtoBufCTempDir

		if err := GenerateCpp(protoFiles, cppTempDir); err != nil {
			logger.Global.Fatal("生成C++代码失败",
				zap.Error(err),
			)
		}

		cppDestDir := _config.Global.Paths.GrpcOutputDir

		if err := CopyCppOutputs(wg, protoFiles, cppTempDir, cppDestDir); err != nil {
			logger.Global.Fatal("拷贝C++代码失败",
				zap.Error(err),
			)
		}

	}()

	return nil
}

// generateGameGrpcImpl 游戏GRPC生成核心逻辑
func generateGameGrpcImpl(wg *sync.WaitGroup) error {
	gameProtoPath, err := _proto.ResolveGameProtoPath()
	if err != nil {
		logger.Global.Fatal("解析Proto路径失败",
			zap.Error(err),
		)
	}
	protoFiles := []string{gameProtoPath}

	if err := generateGameGrpcCpp(wg, protoFiles); err != nil {
		logger.Global.Fatal("C++代码生成失败",
			zap.Error(err),
		)
	}

	return nil
}

// GenerateGameGrpc 生成游戏GRPC代码（C++序列化+Go节点代码）
func GenerateGameGrpc(wg *sync.WaitGroup) {
	wg.Add(1)
	go func() {
		defer wg.Done() // 修正：原代码wg.Done()写在err判断前，会导致提前释放
		if err := generateGameGrpcImpl(wg); err != nil {
			logger.Global.Warn("游戏GRPC生成: 整体失败",
				zap.Error(err),
			)
		}
	}()
}

func GeneratorHandler(wg *sync.WaitGroup) {
	for _, service := range internal.GlobalRPCServiceList {
		ProcessAllHandlers(wg, service.Methods)
	}
}

func WriteMethodFile(wg *sync.WaitGroup) {
	// Concurrent operations for game, centre, and gate registers
	wg.Add(1)
	go internal.GenRegisterFile(wg, _config.Global.PathLists.MethodHandlerDirectories.SceneNode+_config.Global.FileExtensions.RegisterHandlerCppExtension, IsSceneNodeHostedProtocolHandler)
	wg.Add(1)
	go internal.WriteRepliedRegisterFile(wg, _config.Global.PathLists.MethodHandlerDirectories.SceneNodeReplied+_config.Global.FileExtensions.RegisterRepliedHandlerCppExtension, IsSceneNodeReceivedProtocolResponseHandler)

	wg.Add(1)
	go internal.GenRegisterFile(wg, _config.Global.PathLists.MethodHandlerDirectories.CentreNode+_config.Global.FileExtensions.RegisterHandlerCppExtension, IsCentreHostedServiceHandler)
	wg.Add(1)
	go internal.WriteRepliedRegisterFile(wg, _config.Global.PathLists.MethodHandlerDirectories.CentreNodeReplied+_config.Global.FileExtensions.RegisterRepliedHandlerCppExtension, IsCentreReceivedServiceResponseHandler)

	wg.Add(1)
	go internal.GenRegisterFile(wg, _config.Global.PathLists.MethodHandlerDirectories.GateNodeReplied+_config.Global.FileExtensions.RegisterHandlerCppExtension, IsNoOpHandler)
	wg.Add(1)
	go internal.WriteRepliedRegisterFile(wg, _config.Global.PathLists.MethodHandlerDirectories.GateNodeReplied+_config.Global.FileExtensions.RegisterRepliedHandlerCppExtension, IsGateNodeReceivedResponseHandler)
}
