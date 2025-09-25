package internal

import (
	"fmt"
	"log"
	"os"
	"path/filepath"
	"pbgen/internal/config"
	"pbgen/utils"
)

// GenerateGameGrpc 生成游戏GRPC代码（C++序列化+Go节点代码）
func GenerateGameGrpc() error {
	utils.Wg.Add(1)
	go func() {
		defer utils.Wg.Done()
		if err := generateGameGrpcImpl(); err != nil {
			log.Printf("游戏GRPC生成: 整体失败: %v", err)
		}
	}()
	return nil
}

// generateGameGrpcImpl 游戏GRPC生成核心逻辑
func generateGameGrpcImpl() error {
	// 1. 解析游戏Proto文件路径
	gameProtoPath, err := resolveGameProtoPath()
	if err != nil {
		return fmt.Errorf("解析Proto路径失败: %w", err)
	}
	protoFiles := []string{gameProtoPath}

	// 2. 生成C++序列化代码
	if err := generateGameGrpcCpp(protoFiles); err != nil {
		return fmt.Errorf("C++代码生成失败: %w", err)
	}

	// 3. 生成Go节点代码
	if err := generateGameGrpcGo(protoFiles); err != nil {
		return fmt.Errorf("Go代码生成失败: %w", err)
	}

	return nil
}

// resolveGameProtoPath 解析游戏核心Proto文件路径
func resolveGameProtoPath() (string, error) {
	gameProtoRoot, err := resolveAbsPath(config.GameRpcProtoPath, "游戏Proto根目录")
	if err != nil {
		return "", err
	}

	gameProtoPath := filepath.Join(gameProtoRoot, config.GameRpcProtoName)
	if _, err := os.Stat(gameProtoPath); err != nil {
		return "", fmt.Errorf("游戏Proto文件不存在: 路径=%s, 错误=%w", gameProtoPath, err)
	}
	return gameProtoPath, nil
}

// generateGameGrpcCpp 生成游戏GRPC C++代码
func generateGameGrpcCpp(protoFiles []string) error {
	// 解析输出目录
	cppOutputDir, err := resolveAbsPath(config.PbcProtoOutputDirectory, "游戏C++输出目录")
	if err != nil {
		return err
	}
	if err := ensureDir(cppOutputDir); err != nil {
		return fmt.Errorf("创建C++输出目录失败: %w", err)
	}

	// 解析临时目录
	cppTempDir, err := resolveAbsPath(config.PbcTempDirectory, "游戏C++临时目录")
	if err != nil {
		return err
	}

	// 生成C++代码
	if err := generateCpp(protoFiles, cppTempDir); err != nil {
		return fmt.Errorf("生成C++代码失败: %w", err)
	}

	// 拷贝C++代码到目标目录
	cppDestDir, err := resolveAbsPath(config.PbcProtoOutputNoProtoSuffixPath, "游戏C++最终目录")
	if err != nil {
		return err
	}
	if err := copyCppOutputs(protoFiles, cppTempDir, cppDestDir); err != nil {
		return fmt.Errorf("拷贝C++代码失败: %w", err)
	}

	return nil
}

// generateGameGrpcGo 为游戏GRPC生成多节点Go代码
func generateGameGrpcGo(protoFiles []string) error {
	// 1. 获取所有GRPC节点目录
	grpcNodes := utils.GetGRPCSubdirectoryNames()
	if len(grpcNodes) == 0 {
		log.Println("Go生成: 未找到GRPC节点目录，跳过")
		return nil
	}

	// 2. 为每个节点生成专属代码
	for _, nodeName := range grpcNodes {
		nodeOutputDir := filepath.Join(config.NodeGoDirectory, nodeName)
		nodeOutputDir, err := resolveAbsPath(nodeOutputDir, "节点game_rpc代码目录")
		if err != nil {
			return fmt.Errorf("解析节点game_rpc代码目录: %w", err)
		}
		if err := ensureDir(nodeOutputDir); err != nil {
			log.Printf("Go生成: 创建节点[%s]目录失败: %v，跳过", nodeName, err)
			continue
		}

		// 生成节点Go GRPC代码
		if err := generateGoGrpc(protoFiles, nodeOutputDir, config.GameRpcProtoPath); err != nil {
			log.Printf("Go生成: 节点[%s]代码生成失败: %v，跳过", nodeName, err)
			continue
		}
		log.Printf("Go生成: 节点[%s]代码生成成功", nodeName)
	}

	// 3. 确保机器人代码目录存在
	robotDir, err := resolveAbsPath(config.RobotGoGamePbDirectory, "机器人代码目录")
	if err != nil {
		return fmt.Errorf("解析机器人目录失败: %w", err)
	}
	if err := ensureDir(robotDir); err != nil {
		return fmt.Errorf("创建机器人目录失败: %w", err)
	}

	if err := generateGoGrpc(protoFiles, robotDir, config.GameRpcProtoPath); err != nil {
		log.Printf("Go生成: 节点[%s]代码生成失败: %v，跳过", robotDir, err)
		return err
	}

	log.Println("Go生成: 所有游戏GRPC节点代码生成完成")
	return nil
}
