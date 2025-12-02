package proto

import (
	"encoding/json"
	"github.com/luyuancpp/protooption"
	"google.golang.org/protobuf/proto"
	"os"
	"path/filepath"
	_config "pbgen/internal/config"
	"strings"
	"sync"

	"google.golang.org/protobuf/types/descriptorpb"
	"pbgen/internal/utils"
)

// MessageListConfig 用于生成消息列表JSON的结构
type MessageListConfig struct {
	Messages []string `json:"messages"`
}

// Processor 协议处理器，仅依赖传入的FileDescriptorSet
type Processor struct {
	fdSet        *descriptorpb.FileDescriptorSet // 仅使用传入的描述符集
	maxGoroutine int
	workerChan   chan struct{}
}

// NewProcessor 创建处理器实例，接收外部传入的FileDescriptorSet
func NewProcessor(fdSet *descriptorpb.FileDescriptorSet, maxGoroutine int) *Processor {
	if maxGoroutine <= 0 {
		maxGoroutine = 10 // 默认并发数
	}
	return &Processor{
		fdSet:        fdSet,
		maxGoroutine: maxGoroutine,
		workerChan:   make(chan struct{}, maxGoroutine),
	}
}

// Process 处理传入的FileDescriptorSet，不依赖内部全局状态
func (p *Processor) Process() error {
	var wg sync.WaitGroup
	errCh := make(chan error, len(p.fdSet.File))

	// 收集所有消息名（仅基于当前fdSet）
	var allMessages []string
	msgMu := sync.Mutex{}

	// 遍历传入的描述符集中的所有文件
	for _, fileDesc := range p.fdSet.File {
		filename := utils.GetBaseName(fileDesc.GetName())
		if _config.Global.ShouldIgnoreFile(filename) {
			continue
		}

		wg.Add(1)
		p.workerChan <- struct{}{}

		go func(fd *descriptorpb.FileDescriptorProto) {
			defer wg.Done()
			defer func() { <-p.workerChan }()

			// 提取文件信息（仅使用当前fd的数据）
			pkgName := fd.GetPackage()
			fileName := utils.GetBaseName(fd.GetName())
			messages := getMessageNames(fd)
			services := getServiceNames(fd)
			options := parseFileOptions(fd)

			// 收集消息名
			msgMu.Lock()
			allMessages = append(allMessages, messages...)
			msgMu.Unlock()

			// 生成DB模型
			if err := p.generateDBModel(pkgName, fileName, messages, options); err != nil {
				errCh <- err
				return
			}

			// 生成服务代码
			if len(services) > 0 {
				if err := p.generateServiceCode(pkgName, fileName, services, options); err != nil {
					errCh <- err
				}
			}
		}(fileDesc)
	}

	// 等待所有任务完成
	go func() {
		wg.Wait()
		close(errCh)
	}()

	// 处理错误
	for err := range errCh {
		if err != nil {
			return err
		}
	}

	// 生成全局消息列表
	return p.writeMessageNamesToJSON(allMessages)
}

// 提取消息名称（仅基于当前FileDescriptorProto）
func getMessageNames(fd *descriptorpb.FileDescriptorProto) []string {
	var names []string
	for _, msg := range fd.MessageType {
		names = append(names, msg.GetName())
	}
	return names
}

// 提取服务名称（仅基于当前FileDescriptorProto）
func getServiceNames(fd *descriptorpb.FileDescriptorProto) []string {
	var names []string
	for _, svc := range fd.Service {
		names = append(names, svc.GetName())
	}
	return names
}

// 解析文件选项（仅基于当前FileDescriptorProto）
func parseFileOptions(fd *descriptorpb.FileDescriptorProto) map[string]interface{} {
	options := make(map[string]interface{})
	if fd.Options != nil {
		// 解析自定义选项（示例）
		if val, ok := proto.GetExtension(fd.GetOptions(), messageoption.E_OptionFileDefaultNode).(string); ok {
			options["file_default_node"] = val
		}
	}
	return options
}

// generateDBModel 生成DB模型代码
func (p *Processor) generateDBModel(pkgName, fileName string, messages []string, options map[string]interface{}) error {
	outputDir := filepath.Join(_config.Global.Paths.OutputRoot, "db", pkgName)
	if err := os.MkdirAll(outputDir, 0755); err != nil {
		return err
	}

	var code strings.Builder
	code.WriteString("package " + pkgName + "\n\n")
	code.WriteString("// 自动生成的DB模型代码，请勿手动修改\n")
	for _, msg := range messages {
		code.WriteString(generateModelStruct(msg))
	}

	outputPath := filepath.Join(outputDir, fileName+"_db.gen.go")
	return os.WriteFile(outputPath, []byte(code.String()), 0644)
}

// generateServiceCode 生成服务代码
func (p *Processor) generateServiceCode(pkgName, fileName string, services []string, options map[string]interface{}) error {
	outputDir := filepath.Join(_config.Global.Paths.OutputRoot, "service", pkgName)
	if err := os.MkdirAll(outputDir, 0755); err != nil {
		return err
	}

	var code strings.Builder
	code.WriteString("package " + pkgName + "\n\n")
	code.WriteString("// 自动生成的服务代码，请勿手动修改\n")
	for _, svc := range services {
		code.WriteString(generateServiceInterface(svc))
	}

	outputPath := filepath.Join(outputDir, fileName+"_service.gen.go")
	return os.WriteFile(outputPath, []byte(code.String()), 0644)
}

// writeMessageNamesToJSON 生成消息列表JSON
func (p *Processor) writeMessageNamesToJSON(messages []string) error {
	data, err := json.MarshalIndent(&MessageListConfig{Messages: messages}, "", "  ")
	if err != nil {
		return err
	}

	outputPath := filepath.Join(
		_config.Global.Paths.TableGeneratorDir,
		_config.Global.Naming.DbTableListJson,
	)
	return os.WriteFile(outputPath, data, 0644)
}

// 生成模型结构体
func generateModelStruct(msgName string) string {
	return "type " + msgName + "Model struct {\n" +
		"    // 自动生成的模型字段\n" +
		"    Id int64 `db:\"id\"`\n" +
		"}\n\n"
}

// 生成服务接口
func generateServiceInterface(svcName string) string {
	return "type " + svcName + "Server interface {\n" +
		"    // 自动生成的服务方法\n" +
		"}\n\n"
}
