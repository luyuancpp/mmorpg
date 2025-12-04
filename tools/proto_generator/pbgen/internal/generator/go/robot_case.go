package _go

import (
	"os"
	"strings"
	"sync"
	"text/template"

	"go.uber.org/zap" // 引入zap结构化日志字段
	"pbgen/internal"
	_config "pbgen/internal/config"
	"pbgen/logger" // 引入全局logger包
)

const handlerTotalTemplate = `package handler

import (
	"go.uber.org/zap"
	"google.golang.org/protobuf/proto"
	"robot/generated/pb/game"
	"robot/proto/common"

	"robot/logic/gameobject"
	"robot/pkg"
)

func MessageBodyHandler(client *pkg.GameClient, response *common.MessageContent) {
	// Log the incoming message body for debugging
	zap.L().Debug("Received message body", zap.String("response", response.String()))

	// Retrieve the player from the player list
	player, ok := gameobject.PlayerList.Get(client.PlayerId)
	if !ok {
		zap.L().Error("Player not found", zap.Uint64("player_id", client.PlayerId))
		return
	}

	// Handle different message types
	switch response.MessageId {
	{{- range .Cases }}
	case game.{{.MessageID}}:
		{{.HandlerFunction}}(player, response.SerializedMessage)
	{{- end }}
	default:
		// Handle unknown message IDs
		zap.L().Info("Unhandled message", zap.Uint32("message_id", response.MessageId), zap.String("response", response.String()))
	}
}

{{- range .Cases }}
func {{.HandlerFunction}}(player *gameobject.Player, body []byte) {
	message := &{{.MessageType}}{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal {{.MessageType}}", zap.Error(err))
		return
	}
	{{.FunctionCall}}(player, message)
}

{{- end }}
`

type HandlerCase struct {
	MessageID       string
	HandlerFunction string
	MessageType     string
	FunctionCall    string
}

type CasesData struct {
	Cases []HandlerCase
}

// isClientMethodRepliedHandler 检查是否为客户端方法已响应处理器
func isClientMethodRepliedHandler(methodList *internal.RPCMethods) bool {
	firstMethodInfo := (*methodList)[0]
	return internal.IsClientProtocolService(firstMethodInfo.ServiceDescriptorProto)
}

func GoRobotTotalHandlerGenerator(wg *sync.WaitGroup) {
	wg.Add(1)
	go func() {
		defer wg.Done()

		handlerCases := make([]HandlerCase, 0)
		for _, service := range internal.GlobalRPCServiceList {
			if !isClientMethodRepliedHandler(&service.MethodInfo) {
				logger.Global.Debug("跳过非客户端响应服务",
					zap.String("service_name", service.GetServiceName()),
				)
				continue
			}

			for _, method := range service.MethodInfo {
				if !isRelevantService(method) {
					logger.Global.Debug("跳过无关服务方法",
						zap.String("service_name", method.Service()),
						zap.String("method_name", method.Method()),
					)
					continue
				}
				handlerCases = generateHandlerCases(method, handlerCases)
			}
		}

		logger.Global.Info("开始生成Robot消息处理器文件",
			zap.Int("handler_case_count", len(handlerCases)),
		)

		err := generateTotalHandlerFile(_config.Global.Paths.RobotMsgBodyHandlerFile, handlerCases)
		if err != nil {
			logger.Global.Error("生成Robot消息处理器文件失败",
				zap.Error(err),
			)
			return
		}

		logger.Global.Info("Robot消息处理器文件生成成功",
			zap.String("file_path", _config.Global.Paths.RobotMsgBodyHandlerFile),
			zap.Int("handler_case_count", len(handlerCases)),
		)
	}()
}

// generateHandlerCases creates the cases for the switch statement based on the method.
func generateHandlerCases(method *internal.MethodInfo, cases []HandlerCase) []HandlerCase {
	handlerCase := HandlerCase{
		MessageID:       method.Service() + method.Method() + _config.Global.Naming.MessageId,
		HandlerFunction: "handle" + method.Service() + method.Method(),
		MessageType:     determineResponseType(method),
		FunctionCall:    method.Service() + method.Method() + "Handler",
	}

	logger.Global.Debug("生成处理器Case",
		zap.String("service_name", method.Service()),
		zap.String("method_name", method.Method()),
		zap.String("message_id", handlerCase.MessageID),
		zap.String("handler_function", handlerCase.HandlerFunction),
		zap.String("message_type", handlerCase.MessageType),
	)

	cases = append(cases, handlerCase)
	return cases
}

// generateHandlerFile creates a new handler file with the specified parameters.
func generateTotalHandlerFile(fileName string, cases []HandlerCase) error {
	// 提前校验文件路径
	if fileName == "" {
		logger.Global.Fatal("生成处理器文件失败: 文件路径为空")
	}

	// 创建文件（如果文件已存在会覆盖，可根据需求调整为os.OpenFile）
	file, err := os.Create(fileName)
	if err != nil {
		logger.Global.Fatal("创建处理器文件失败",
			zap.String("file_name", fileName),
			zap.Error(err),
		)
	}
	defer file.Close()

	// 解析模板
	tmpl, err := template.New("handler").Parse(handlerTotalTemplate)
	if err != nil {
		logger.Global.Fatal("解析处理器模板失败",
			zap.Error(err),
		)
	}

	// 准备模板数据
	data := CasesData{
		Cases: cases,
	}

	// 执行模板并写入文件
	if err := tmpl.Execute(file, data); err != nil {
		logger.Global.Fatal("执行处理器模板失败",
			zap.String("file_name", fileName),
			zap.Int("case_count", len(cases)),
			zap.Error(err),
		)
	}

	logger.Global.Debug("处理器模板执行完成",
		zap.String("file_name", fileName),
		zap.Int("case_count", len(cases)),
	)

	return nil
}

// isRelevantService checks if the service name is relevant.
func isRelevantService(method *internal.MethodInfo) bool {
	isRelevant := strings.Contains(method.Service(), "GamePlayer") || strings.Contains(method.Service(), "ClientPlayer")

	if !isRelevant {
		logger.Global.Debug("服务方法不相关，跳过",
			zap.String("service_name", method.Service()),
			zap.String("method_name", method.Method()),
		)
	}

	return isRelevant
}

// determineResponseType returns the response type or request type based on configuration.
func determineResponseType(method *internal.MethodInfo) string {
	responseType := method.GoResponse()

	if strings.Contains(responseType, _config.Global.Naming.EmptyResponse) {
		responseType = method.GoRequest()
		logger.Global.Debug("响应类型为空，使用请求类型",
			zap.String("service_name", method.Service()),
			zap.String("method_name", method.Method()),
			zap.String("original_response_type", method.GoResponse()),
			zap.String("final_type", responseType),
		)
	}

	return responseType
}
