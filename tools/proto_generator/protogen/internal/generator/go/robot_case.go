package _go

import (
	"os"
	"strings"
	"sync"
	"text/template"

	"protogen/internal"
	_config "protogen/internal/config"
	"protogen/logger"

	"go.uber.org/zap"
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

// isClientMethodRepliedHandler checks if this is a client method replied handler.
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
			if !isClientMethodRepliedHandler(&service.Methods) {
				logger.Global.Debug("Skipping non-client-response service",
					zap.String("service_name", service.Service()),
				)
				continue
			}

			for _, method := range service.Methods {
				if !isRelevantService(method) {
					logger.Global.Debug("Skipping irrelevant service method",
						zap.String("service_name", method.Service()),
						zap.String("method_name", method.Method()),
					)
					continue
				}
				handlerCases = generateHandlerCases(method, handlerCases)
			}
		}

		logger.Global.Info("Generating robot message handler file",
			zap.Int("handler_case_count", len(handlerCases)),
		)

		err := generateTotalHandlerFile(_config.Global.Paths.RobotMsgBodyHandlerFile, handlerCases)
		if err != nil {
			logger.Global.Error("Failed to generate robot message handler file",
				zap.Error(err),
			)
			return
		}

		logger.Global.Info("Robot message handler file generated",
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

	logger.Global.Debug("Generated handler case",
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
	if fileName == "" {
		logger.Global.Fatal("Failed to generate handler file: file path is empty")
	}

	file, err := os.Create(fileName)
	if err != nil {
		logger.Global.Fatal("Failed to create handler file",
			zap.String("file_name", fileName),
			zap.Error(err),
		)
	}
	defer file.Close()

	tmpl, err := template.New("handler").Parse(handlerTotalTemplate)
	if err != nil {
		logger.Global.Fatal("Failed to parse handler template",
			zap.Error(err),
		)
	}

	data := CasesData{
		Cases: cases,
	}

	if err := tmpl.Execute(file, data); err != nil {
		logger.Global.Fatal("Failed to execute handler template",
			zap.String("file_name", fileName),
			zap.Int("case_count", len(cases)),
			zap.Error(err),
		)
	}

	logger.Global.Debug("Handler template executed",
		zap.String("file_name", fileName),
		zap.Int("case_count", len(cases)),
	)

	return nil
}

// isRelevantService checks if the service name is relevant.
func isRelevantService(method *internal.MethodInfo) bool {
	isRelevant := strings.Contains(method.Service(), "GamePlayer") || strings.Contains(method.Service(), "ClientPlayer")

	if !isRelevant {
		logger.Global.Debug("Service method is irrelevant, skipping",
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
		logger.Global.Debug("Response type is empty, using request type",
			zap.String("service_name", method.Service()),
			zap.String("method_name", method.Method()),
			zap.String("original_response_type", method.GoResponse()),
			zap.String("final_type", responseType),
		)
	}

	return responseType
}
