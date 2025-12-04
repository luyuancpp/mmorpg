package _go

import (
	"os"
	"path/filepath"
	"strings"
	"sync"
	"text/template"

	"go.uber.org/zap" // 引入zap结构化日志字段
	"pbgen/internal"
	_config "pbgen/internal/config"
	"pbgen/logger" // 引入全局logger包
)

const handlerTemplate = `package handler

import (
	"robot/pb/game"
	"robot/logic/gameobject"
)

func {{.HandlerName}}(player *gameobject.Player, response *game.{{.ResponseType}}) {
}
`

type ServiceData struct {
	HandlerName  string
	ResponseType string
}

// GoRobotHandlerGenerator generates Go handler files and removes obsolete ones.
func GoRobotHandlerGenerator(wg *sync.WaitGroup) {
	logger.Global.Info("开始生成Robot单个处理器文件",
		zap.Int("total_service_count", len(internal.GlobalRPCServiceList)),
	)

	for _, service := range internal.GlobalRPCServiceList {
		wg.Add(1)
		go func(methods internal.RPCMethods) {
			defer wg.Done()

			// 跳过非客户端响应处理器
			if !isClientMethodRepliedHandler(&methods) {
				logger.Global.Debug("跳过非客户端响应服务的处理器生成",
					zap.Int("method_count", len(methods)),
				)
				return
			}

			// 遍历每个方法生成处理器文件
			for _, method := range methods {
				serviceName := method.Service()

				// 跳过无关服务
				if !isRelevantService(method) {
					logger.Global.Debug("跳过无关服务方法的处理器生成",
						zap.String("service_name", serviceName),
						zap.String("method_name", method.Method()),
					)
					continue
				}

				// 生成处理器函数名和响应类型
				handlerName := serviceName + method.Method() + "Handler"
				responseType := method.GoResponse()

				// 空响应类型替换为请求类型
				if strings.Contains(responseType, _config.Global.Naming.EmptyResponse) {
					responseType = method.GoRequest()
					logger.Global.Debug("响应类型为空，替换为请求类型",
						zap.String("service_name", serviceName),
						zap.String("method_name", method.Method()),
						zap.String("original_response_type", method.GoResponse()),
						zap.String("final_response_type", responseType),
					)
				}

				// 生成合法文件名并拼接路径
				fileName := sanitizeFileName(serviceName + method.Method())
				filePath := filepath.Join(_config.Global.PathLists.MethodHandlerDirectories.Robot, fileName+".go")

				// 文件已存在则跳过
				if fileExists(filePath) {
					logger.Global.Debug("处理器文件已存在，跳过生成",
						zap.String("file_path", filePath),
						zap.String("service_name", serviceName),
						zap.String("method_name", method.Method()),
					)
					continue
				}

				// 生成处理器文件
				err := generateHandlerFile(filePath, handlerName, responseType)
				if err != nil {
					logger.Global.Warn("生成处理器文件失败，跳过",
						zap.String("file_path", filePath),
						zap.String("service_name", serviceName),
						zap.String("method_name", method.Method()),
						zap.Error(err),
					)
					continue
				}

				logger.Global.Info("处理器文件生成成功",
					zap.String("file_path", filePath),
					zap.String("handler_name", handlerName),
					zap.String("response_type", responseType),
				)
			}
		}(service.MethodInfo)
	}

	logger.Global.Info("Robot单个处理器文件生成任务已提交",
		zap.Int("service_count", len(internal.GlobalRPCServiceList)),
	)
}

// generateHandlerFile Generates a Go file using the provided handler and response names.
func generateHandlerFile(fileName, handlerName, responseType string) error {
	// 空路径校验
	if fileName == "" {
		logger.Global.Fatal("生成处理器文件失败: 文件路径为空")
	}

	// 创建文件（存在则覆盖）
	file, err := os.Create(fileName)
	if err != nil {
		logger.Global.Fatal("创建处理器文件失败",
			zap.String("file_name", fileName),
			zap.Error(err),
		)
	}
	defer file.Close()

	// 解析模板
	tmpl, err := template.New("handler").Parse(handlerTemplate)
	if err != nil {
		logger.Global.Fatal("解析处理器模板失败",
			zap.String("file_name", fileName),
			zap.Error(err),
		)
	}

	// 准备模板数据
	data := ServiceData{
		HandlerName:  handlerName,
		ResponseType: responseType,
	}

	logger.Global.Debug("执行处理器模板渲染",
		zap.String("file_name", fileName),
		zap.String("handler_name", handlerName),
		zap.String("response_type", responseType),
	)

	// 写入模板内容到文件
	return tmpl.Execute(file, data)
}

// fileExists Checks if a file already exists.
func fileExists(filePath string) bool {
	_, err := os.Stat(filePath)
	if err != nil {
		if os.IsNotExist(err) {
			return false
		}
		// 其他错误（如权限问题）也视为"存在"（避免重复创建）
		logger.Global.Warn("检查文件存在性时出错",
			zap.String("file_path", filePath),
			zap.Error(err),
		)
		return true
	}
	return true
}

// sanitizeFileName replaces invalid characters in service names for valid file names.
func sanitizeFileName(serviceName string) string {
	// 替换无效字符并转为小写
	sanitized := strings.ToLower(strings.ReplaceAll(serviceName, " ", "_"))
	logger.Global.Debug("文件名清洗完成",
		zap.String("original_name", serviceName),
		zap.String("sanitized_name", sanitized),
	)
	return sanitized
}
